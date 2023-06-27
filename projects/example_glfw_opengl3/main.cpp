// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <iostream>
#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif

#include <ImFileBrowser.h>

#include <opencv2/opencv.hpp>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

using namespace std;

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

bool decodeFrame(AVFormatContext *formatContext, int videoStreamIndex, AVFrame *&outputFrame, int targetFrameCount)
{
    // Get the codec and codec context
    AVCodecParameters *codecParameters = formatContext->streams[videoStreamIndex]->codecpar;
    AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);
    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, codecParameters);

    // Open the codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0)
    {
        std::cerr << "Could not open codec.\n";
        return false;
    }

    AVFrame *frame = av_frame_alloc();
    AVPacket packet;
    int frameCount = 0;

    // SwsContext *img_convert_ctx = sws_getContext(codecContext->width,
    //             codecContext->height, codecContext->pix_fmt, codecContext->width,
    //             codecContext->height, PIX_FMT_RGB24, SWS_BICUBIC, NULL,
    //             NULL, NULL);

    // Read the frames
    while (av_read_frame(formatContext, &packet) >= 0)
    {
        if (packet.stream_index == videoStreamIndex)
        {
            // Decode the video frame
            if (avcodec_send_packet(codecContext, &packet) < 0)
            {
                std::cerr << "Error sending packet for decoding.\n";
                return false;
            }

            int ret = avcodec_receive_frame(codecContext, frame);
            if (ret == 0)
            {
                // Stop decoding when the third frame is found
                if (frameCount++ == targetFrameCount)
                {
                    // outputFrame = frame;

                    // Convert ffmpeg frame timestamp to real frame number.
                    // int64_t numberFrame = (double)((int64_t)pts -
                    // 	pFormatCtx->streams[videoStreamIndex]->start_time) *
                    // 	videoBaseTime * videoFramePerSecond;

                    // Get RGBA Frame
                    AVFrame *rgbaFrame = NULL;
                    int width = codecContext->width;
                    int height = codecContext->height;
                    int bufferImgSize = avpicture_get_size(AV_PIX_FMT_BGR24, width, height);
                    rgbaFrame = av_frame_alloc();
                    uint8_t *buffer = (uint8_t *)av_mallocz(bufferImgSize);
                    if (rgbaFrame)
                    {
                        avpicture_fill((AVPicture *)rgbaFrame, buffer, AV_PIX_FMT_BGR24, width, height);
                        rgbaFrame->width = width;
                        rgbaFrame->height = height;
                        // rgbaFrame->data[0] = buffer;

                        SwsContext *pImgConvertCtx = sws_getContext(codecContext->width, codecContext->height,
                                                                    codecContext->pix_fmt,
                                                                    codecContext->width, codecContext->height,
                                                                    AV_PIX_FMT_BGR24,
                                                                    SWS_BICUBIC, NULL, NULL, NULL);

                        sws_scale(pImgConvertCtx, frame->data, frame->linesize,
                                  0, height, rgbaFrame->data, rgbaFrame->linesize);
                    }

                    outputFrame = (AVFrame *)rgbaFrame;
                    break;
                }
            }
        }
        av_packet_unref(&packet);
    }

    // Clean up
    av_packet_unref(&packet);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);

    return frameCount == targetFrameCount + 1;
}

GLuint createTextureFromFrame(AVFrame *frame)
{
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // // glPixelStorei(GL_UNPACK_ALIGNMENT, 2);

    // GLenum format = frame->format == AV_PIX_FMT_RGB24 ? GL_RGB : GL_RGBA;
    // glTexImage2D(GL_TEXTURE_2D, 0, format, frame->width, frame->height, 0, format, GL_UNSIGNED_BYTE, frame->data[0]);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload the frame data to the texture object
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame->width, frame->height, 0, GL_RGB, GL_UNSIGNED_BYTE, frame->data[0]);

    return textureId;
}

// Main code
int main(int, char **)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

        // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char *glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow *window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // FileBrowser
    ImGui::FileBrowser fileDialog;
    fileDialog.SetTitle("Select the .dcm file to be converted");
    fileDialog.SetTypeFilters({".dcm"});

    // Display mp4
    bool showVideo = false;
    double currentIoFrameNumber = 0;

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");          // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window); // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
            {
                counter++;

                // FileBrowser
                fileDialog.Open();
            }

            // FileBrowser
            fileDialog.Display();
            if (fileDialog.HasSelected())
            {
                cout << "Selected filename is " << fileDialog.GetSelected().string() << endl;

                cout << "Current path is " << filesystem::current_path() << endl; // (1)
                system("chmod +x /echocardiography-ui/packages/DICOMTestExe/DICOMTestExe");
                string command = "yes | /echocardiography-ui/packages/DICOMTestExe/DICOMTestExe " + fileDialog.GetSelected().string() + " A2C";
                system(command.c_str());

                fileDialog.ClearSelected();

                showVideo = true;
            }

            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        if (showVideo)
        {
            // Read mp4 file
            // Initialize FFmpeg
            av_register_all();

            // Open the input file
            AVFormatContext *formatContext = nullptr;
            std::string inputFilename = "/echocardiography-ui/packages/DICOMTestExe/data/dcm/dicomresults/A2C/mp4s/PWHOR190734217S_12Oct2021_CX03WQDU_3DQ.mp4";
            if (avformat_open_input(&formatContext, inputFilename.c_str(), nullptr, nullptr) != 0)
            {
                std::cerr << "Could not open input file.\n";
                return false;
            }

            // Find the first video stream
            int videoStreamIndex = -1;
            for (unsigned int i = 0; i < formatContext->nb_streams; ++i)
            {
                if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                {
                    videoStreamIndex = i;
                    break;
                }
            }

            if (videoStreamIndex == -1)
            {
                std::cerr << "No video stream found.\n";
                return false;
            }

            int64_t nb_frames = formatContext->streams[videoStreamIndex]->nb_frames;
            cout << "nb_frames: " << nb_frames << endl;
            int64_t duration = formatContext->streams[videoStreamIndex]->duration;
            double durationInSeconds = (double)duration * av_q2d(formatContext->streams[videoStreamIndex]->time_base);
            cout << "durationInSeconds: " << durationInSeconds << endl;
            double frameRate = av_q2d(formatContext->streams[videoStreamIndex]->r_frame_rate);
            cout << "frameRate: " << frameRate << endl;

            // Decode frame
            ImGui::Begin("Video Player");
            currentIoFrameNumber = currentIoFrameNumber + frameRate / io.Framerate;
            if (currentIoFrameNumber > 90)
            {
                currentIoFrameNumber -= 90;
            }
            if (currentIoFrameNumber < nb_frames)
            {
                AVFrame *targetFrame = nullptr;
                if (!decodeFrame(formatContext, videoStreamIndex, targetFrame, currentIoFrameNumber))
                {
                    std::cerr << "Could not decode the target frame.\n";
                    return -1;
                }

                GLuint textureId = createTextureFromFrame(targetFrame);
                av_frame_free(&targetFrame);

                ImVec2 window_size = ImGui::GetWindowSize();
                ImGui::Image((void *)(intptr_t)textureId, window_size);
            }
            ImGui::End();
        }

        // Display video information
        // ImGui::Text("Video resolution: %fx%f", cap.get(cv::CAP_PROP_FRAME_WIDTH), cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        // ImGui::Text("Current frame: %f / %f", cap.get(cv::CAP_PROP_POS_FRAMES), cap.get(cv::CAP_PROP_FRAME_COUNT));

        // // Display controls
        // if (ImGui::Button("Play"))
        // {
        //     cap.set(cv::CAP_PROP_POS_FRAMES, 0);
        // }
        // if (ImGui::Button("Pause"))
        // {
        //     // Do nothing
        // }
        // if (ImGui::Button("Stop"))
        // {
        //     cap.set(cv::CAP_PROP_POS_FRAMES, 0);
        // }
        // ImGui::SliderFloat("Speed", nullptr, 0.1f, 2.0f);

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Cleanup the texture
        // glDeleteTextures(1, &texture_id);

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow *backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
