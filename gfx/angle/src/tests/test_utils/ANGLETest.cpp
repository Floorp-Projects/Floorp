//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLETest:
//   Implementation of common ANGLE testing fixture.
//

#include "ANGLETest.h"
#include "EGLWindow.h"
#include "OSWindow.h"
#include "system_utils.h"

namespace angle
{

GLColor::GLColor() : R(0), G(0), B(0), A(0)
{
}

GLColor::GLColor(GLubyte r, GLubyte g, GLubyte b, GLubyte a) : R(r), G(g), B(b), A(a)
{
}

GLColor::GLColor(GLuint colorValue) : R(0), G(0), B(0), A(0)
{
    memcpy(&R, &colorValue, sizeof(GLuint));
}

GLColor ReadColor(GLint x, GLint y)
{
    GLColor actual;
    glReadPixels((x), (y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &actual.R);
    EXPECT_GL_NO_ERROR();
    return actual;
}

bool operator==(const GLColor &a, const GLColor &b)
{
    return a.R == b.R && a.G == b.G && a.B == b.B && a.A == b.A;
}

std::ostream &operator<<(std::ostream &ostream, const GLColor &color)
{
    ostream << "(" << static_cast<unsigned int>(color.R) << ", "
            << static_cast<unsigned int>(color.G) << ", " << static_cast<unsigned int>(color.B)
            << ", " << static_cast<unsigned int>(color.A) << ")";
    return ostream;
}

}  // namespace angle

ANGLETest::ANGLETest()
    : mEGLWindow(nullptr), mWidth(16), mHeight(16), mIgnoreD3D11SDKLayersWarnings(false)
{
    mEGLWindow =
        new EGLWindow(GetParam().majorVersion, GetParam().minorVersion, GetParam().eglParameters);
}

ANGLETest::~ANGLETest()
{
    SafeDelete(mEGLWindow);
}

void ANGLETest::SetUp()
{
    // Resize the window before creating the context so that the first make current
    // sets the viewport and scissor box to the right size.
    bool needSwap = false;
    if (mOSWindow->getWidth() != mWidth || mOSWindow->getHeight() != mHeight)
    {
        if (!mOSWindow->resize(mWidth, mHeight))
        {
            FAIL() << "Failed to resize ANGLE test window.";
        }
        needSwap = true;
    }

    if (!createEGLContext())
    {
        FAIL() << "egl context creation failed.";
    }

    if (needSwap)
    {
        // Swap the buffers so that the default framebuffer picks up the resize
        // which will allow follow-up test code to assume the framebuffer covers
        // the whole window.
        swapBuffers();
    }

    // This Viewport command is not strictly necessary but we add it so that programs
    // taking OpenGL traces can guess the size of the default framebuffer and show it
    // in their UIs
    glViewport(0, 0, mWidth, mHeight);

    const auto &info = testing::UnitTest::GetInstance()->current_test_info();
    angle::WriteDebugMessage("Entering %s.%s\n", info->test_case_name(), info->name());
}

void ANGLETest::TearDown()
{
    checkD3D11SDKLayersMessages();

    const auto &info = testing::UnitTest::GetInstance()->current_test_info();
    angle::WriteDebugMessage("Exiting %s.%s\n", info->test_case_name(), info->name());

    swapBuffers();
    mOSWindow->messageLoop();

    if (!destroyEGLContext())
    {
        FAIL() << "egl context destruction failed.";
    }

    // Check for quit message
    Event myEvent;
    while (mOSWindow->popEvent(&myEvent))
    {
        if (myEvent.Type == Event::EVENT_CLOSED)
        {
            exit(0);
        }
    }
}

void ANGLETest::swapBuffers()
{
    if (mEGLWindow->isGLInitialized())
    {
        mEGLWindow->swap();
    }
}

void ANGLETest::drawQuad(GLuint program,
                         const std::string &positionAttribName,
                         GLfloat positionAttribZ)
{
    drawQuad(program, positionAttribName, positionAttribZ, 1.0f);
}

void ANGLETest::drawQuad(GLuint program,
                         const std::string &positionAttribName,
                         GLfloat positionAttribZ,
                         GLfloat positionAttribXYScale)
{
    GLint positionLocation = glGetAttribLocation(program, positionAttribName.c_str());

    glUseProgram(program);

    const GLfloat vertices[] = {
        -1.0f * positionAttribXYScale,  1.0f * positionAttribXYScale, positionAttribZ,
        -1.0f * positionAttribXYScale, -1.0f * positionAttribXYScale, positionAttribZ,
         1.0f * positionAttribXYScale, -1.0f * positionAttribXYScale, positionAttribZ,

        -1.0f * positionAttribXYScale,  1.0f * positionAttribXYScale, positionAttribZ,
         1.0f * positionAttribXYScale, -1.0f * positionAttribXYScale, positionAttribZ,
         1.0f * positionAttribXYScale,  1.0f * positionAttribXYScale, positionAttribZ,
    };

    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(positionLocation);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, NULL);

    glUseProgram(0);
}

GLuint ANGLETest::compileShader(GLenum type, const std::string &source)
{
    GLuint shader = glCreateShader(type);

    const char *sourceArray[1] = { source.c_str() };
    glShaderSource(shader, 1, sourceArray, NULL);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);

    if (compileResult == 0)
    {
        GLint infoLogLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

        if (infoLogLength == 0)
        {
            std::cerr << "shader compilation failed with empty log." << std::endl;
        }
        else
        {
            std::vector<GLchar> infoLog(infoLogLength);
            glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), NULL, &infoLog[0]);

            std::cerr << "shader compilation failed: " << &infoLog[0];
        }

        glDeleteShader(shader);
        shader = 0;
    }

    return shader;
}

void ANGLETest::checkD3D11SDKLayersMessages()
{
#if defined(ANGLE_PLATFORM_WINDOWS) && !defined(NDEBUG)
    // In debug D3D11 mode, check ID3D11InfoQueue to see if any D3D11 SDK Layers messages
    // were outputted by the test
    if (mIgnoreD3D11SDKLayersWarnings ||
        mEGLWindow->getPlatform().renderer != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE ||
        mEGLWindow->getDisplay() == EGL_NO_DISPLAY)
    {
        return;
    }

    const char *extensionString =
        static_cast<const char *>(eglQueryString(mEGLWindow->getDisplay(), EGL_EXTENSIONS));
    if (!strstr(extensionString, "EGL_EXT_device_query"))
    {
        return;
    }

    EGLAttrib device      = 0;
    EGLAttrib angleDevice = 0;

    PFNEGLQUERYDISPLAYATTRIBEXTPROC queryDisplayAttribEXT;
    PFNEGLQUERYDEVICEATTRIBEXTPROC queryDeviceAttribEXT;

    queryDisplayAttribEXT = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
        eglGetProcAddress("eglQueryDisplayAttribEXT"));
    queryDeviceAttribEXT = reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(
        eglGetProcAddress("eglQueryDeviceAttribEXT"));
    ASSERT_NE(nullptr, queryDisplayAttribEXT);
    ASSERT_NE(nullptr, queryDeviceAttribEXT);

    ASSERT_EGL_TRUE(queryDisplayAttribEXT(mEGLWindow->getDisplay(), EGL_DEVICE_EXT, &angleDevice));
    ASSERT_EGL_TRUE(queryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice),
                                         EGL_D3D11_DEVICE_ANGLE, &device));
    ID3D11Device *d3d11Device = reinterpret_cast<ID3D11Device *>(device);

    ID3D11InfoQueue *infoQueue = nullptr;
    HRESULT hr =
        d3d11Device->QueryInterface(__uuidof(infoQueue), reinterpret_cast<void **>(&infoQueue));
    if (SUCCEEDED(hr))
    {
        UINT64 numStoredD3DDebugMessages =
            infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();

        if (numStoredD3DDebugMessages > 0)
        {
            for (UINT64 i = 0; i < numStoredD3DDebugMessages; i++)
            {
                SIZE_T messageLength = 0;
                hr                   = infoQueue->GetMessage(i, nullptr, &messageLength);

                if (SUCCEEDED(hr))
                {
                    D3D11_MESSAGE *pMessage =
                        reinterpret_cast<D3D11_MESSAGE *>(malloc(messageLength));
                    infoQueue->GetMessage(i, pMessage, &messageLength);

                    std::cout << "Message " << i << ":"
                              << " " << pMessage->pDescription << "\n";
                    free(pMessage);
                }
            }

            FAIL() << numStoredD3DDebugMessages
                   << " D3D11 SDK Layers message(s) detected! Test Failed.\n";
        }
    }

    SafeRelease(infoQueue);
#endif
}

static bool checkExtensionExists(const char *allExtensions, const std::string &extName)
{
    return strstr(allExtensions, extName.c_str()) != nullptr;
}

bool ANGLETest::extensionEnabled(const std::string &extName)
{
    return checkExtensionExists(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                extName);
}

bool ANGLETest::eglDisplayExtensionEnabled(EGLDisplay display, const std::string &extName)
{
    return checkExtensionExists(eglQueryString(display, EGL_EXTENSIONS), extName);
}

bool ANGLETest::eglClientExtensionEnabled(const std::string &extName)
{
    return checkExtensionExists(eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS), extName);
}

void ANGLETest::setWindowWidth(int width)
{
    mWidth = width;
}

void ANGLETest::setWindowHeight(int height)
{
    mHeight = height;
}

void ANGLETest::setConfigRedBits(int bits)
{
    mEGLWindow->setConfigRedBits(bits);
}

void ANGLETest::setConfigGreenBits(int bits)
{
    mEGLWindow->setConfigGreenBits(bits);
}

void ANGLETest::setConfigBlueBits(int bits)
{
    mEGLWindow->setConfigBlueBits(bits);
}

void ANGLETest::setConfigAlphaBits(int bits)
{
    mEGLWindow->setConfigAlphaBits(bits);
}

void ANGLETest::setConfigDepthBits(int bits)
{
    mEGLWindow->setConfigDepthBits(bits);
}

void ANGLETest::setConfigStencilBits(int bits)
{
    mEGLWindow->setConfigStencilBits(bits);
}

void ANGLETest::setMultisampleEnabled(bool enabled)
{
    mEGLWindow->setMultisample(enabled);
}

void ANGLETest::setDebugEnabled(bool enabled)
{
    mEGLWindow->setDebugEnabled(enabled);
}

void ANGLETest::setNoErrorEnabled(bool enabled)
{
    mEGLWindow->setNoErrorEnabled(enabled);
}

int ANGLETest::getClientVersion() const
{
    return mEGLWindow->getClientMajorVersion();
}

EGLWindow *ANGLETest::getEGLWindow() const
{
    return mEGLWindow;
}

int ANGLETest::getWindowWidth() const
{
    return mWidth;
}

int ANGLETest::getWindowHeight() const
{
    return mHeight;
}

bool ANGLETest::isMultisampleEnabled() const
{
    return mEGLWindow->isMultisample();
}

bool ANGLETest::createEGLContext()
{
    return mEGLWindow->initializeGL(mOSWindow);
}

bool ANGLETest::destroyEGLContext()
{
    mEGLWindow->destroyGL();
    return true;
}

bool ANGLETest::InitTestWindow()
{
    mOSWindow = CreateOSWindow();
    if (!mOSWindow->initialize("ANGLE_TEST", 128, 128))
    {
        return false;
    }

    mOSWindow->setVisible(true);

    return true;
}

bool ANGLETest::DestroyTestWindow()
{
    if (mOSWindow)
    {
        mOSWindow->destroy();
        delete mOSWindow;
        mOSWindow = NULL;
    }

    return true;
}

void ANGLETest::SetWindowVisible(bool isVisible)
{
    mOSWindow->setVisible(isVisible);
}

bool ANGLETest::isIntel() const
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Intel") != std::string::npos);
}

bool ANGLETest::isAMD() const
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("AMD") != std::string::npos) ||
           (rendererString.find("ATI") != std::string::npos);
}

bool ANGLETest::isNVidia() const
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("NVIDIA") != std::string::npos);
}

bool ANGLETest::isD3D11() const
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Direct3D11 vs_5_0") != std::string::npos);
}

bool ANGLETest::isD3D11_FL93() const
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Direct3D11 vs_4_0_") != std::string::npos);
}

bool ANGLETest::isD3D9() const
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return (rendererString.find("Direct3D9") != std::string::npos);
}

bool ANGLETest::isD3DSM3() const
{
    std::string rendererString(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return isD3D9() || isD3D11_FL93();
}

bool ANGLETest::isOSX() const
{
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}

EGLint ANGLETest::getPlatformRenderer() const
{
    assert(mEGLWindow);
    return mEGLWindow->getPlatform().renderer;
}

void ANGLETest::ignoreD3D11SDKLayersWarnings()
{
    // Some tests may need to disable the D3D11 SDK Layers Warnings checks
    mIgnoreD3D11SDKLayersWarnings = true;
}

OSWindow *ANGLETest::mOSWindow = NULL;

void ANGLETestEnvironment::SetUp()
{
    if (!ANGLETest::InitTestWindow())
    {
        FAIL() << "Failed to create ANGLE test window.";
    }
}

void ANGLETestEnvironment::TearDown()
{
    ANGLETest::DestroyTestWindow();
}
