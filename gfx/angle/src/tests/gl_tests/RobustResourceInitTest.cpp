//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RobustResourceInitTest: Tests for GL_ANGLE_robust_resource_initialization.

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

namespace angle
{

class RobustResourceInitTest : public ANGLETest
{
  protected:
    constexpr static int kWidth  = 128;
    constexpr static int kHeight = 128;

    RobustResourceInitTest()
    {
        setWindowWidth(kWidth);
        setWindowHeight(kHeight);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    bool hasEGLExtension()
    {
        return eglClientExtensionEnabled("EGL_ANGLE_display_robust_resource_initialization");
    }

    bool hasGLExtension() { return extensionEnabled("GL_ANGLE_robust_resource_initialization"); }

    bool setup()
    {
        if (!hasEGLExtension())
        {
            return false;
        }

        TearDown();
        setRobustResourceInit(true);
        SetUp();

        return true;
    }

    void setupTexture(GLTexture *tex);
    void setup3DTexture(GLTexture *tex);

    // Checks for uninitialized (non-zero pixels) in a Texture.
    void checkNonZeroPixels(GLTexture *texture,
                            int skipX,
                            int skipY,
                            int skipWidth,
                            int skipHeight,
                            const GLColor &skip);
    void checkNonZeroPixels3D(GLTexture *texture,
                              int skipX,
                              int skipY,
                              int skipWidth,
                              int skipHeight,
                              const GLColor &skip);
    void checkFramebufferNonZeroPixels(int skipX,
                                       int skipY,
                                       int skipWidth,
                                       int skipHeight,
                                       const GLColor &skip);
};

// Display creation should fail if EGL_ANGLE_display_robust_resource_initialization
// is not available, and succeed otherwise.
TEST_P(RobustResourceInitTest, ExtensionInit)
{
    if (setup())
    {
        // Robust resource init extension should be available.
        EXPECT_TRUE(hasGLExtension());

        // Querying the state value should return true.
        GLboolean enabled = 0;
        glGetBooleanv(GL_CONTEXT_ROBUST_RESOURCE_INITIALIZATION_ANGLE, &enabled);
        EXPECT_GL_NO_ERROR();
        EXPECT_GL_TRUE(enabled);

        EXPECT_GL_TRUE(glIsEnabled(GL_CONTEXT_ROBUST_RESOURCE_INITIALIZATION_ANGLE));
    }
    else
    {
        // If context extension string exposed, check queries.
        if (hasGLExtension())
        {
            GLboolean enabled = 0;
            glGetBooleanv(GL_CONTEXT_ROBUST_RESOURCE_INITIALIZATION_ANGLE, &enabled);
            EXPECT_GL_FALSE(enabled);

            EXPECT_GL_FALSE(glIsEnabled(GL_CONTEXT_ROBUST_RESOURCE_INITIALIZATION_ANGLE));
            EXPECT_GL_NO_ERROR();
        }
        else
        {
            // Querying robust resource init should return INVALID_ENUM.
            GLboolean enabled = 0;
            glGetBooleanv(GL_CONTEXT_ROBUST_RESOURCE_INITIALIZATION_ANGLE, &enabled);
            EXPECT_GL_ERROR(GL_INVALID_ENUM);
        }
    }
}

// Test queries on a normal, non-robust enabled context.
TEST_P(RobustResourceInitTest, QueriesOnNonRobustContext)
{
    EGLDisplay display = getEGLWindow()->getDisplay();
    ASSERT_TRUE(display != EGL_NO_DISPLAY);

    if (!hasEGLExtension())
    {
        return;
    }

    // If context extension string exposed, check queries.
    ASSERT_TRUE(hasGLExtension());

    // Querying robust resource init should return INVALID_ENUM.
    GLboolean enabled = 0;
    glGetBooleanv(GL_CONTEXT_ROBUST_RESOURCE_INITIALIZATION_ANGLE, &enabled);
    EXPECT_GL_FALSE(enabled);

    EXPECT_GL_FALSE(glIsEnabled(GL_CONTEXT_ROBUST_RESOURCE_INITIALIZATION_ANGLE));
    EXPECT_GL_NO_ERROR();
}

// Tests that buffers start zero-filled if the data pointer is null.
TEST_P(RobustResourceInitTest, BufferData)
{
    if (!setup())
    {
        return;
    }

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, getWindowWidth() * getWindowHeight() * sizeof(GLfloat), nullptr,
                 GL_STATIC_DRAW);

    const std::string &vertexShader =
        "attribute vec2 position;\n"
        "attribute float testValue;\n"
        "varying vec4 colorOut;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "    colorOut = testValue == 0.0 ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n"
        "}";
    const std::string &fragmentShader =
        "varying mediump vec4 colorOut;\n"
        "void main() {\n"
        "    gl_FragColor = colorOut;\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    GLint testValueLoc = glGetAttribLocation(program.get(), "testValue");
    ASSERT_NE(-1, testValueLoc);

    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(testValueLoc, 1, GL_FLOAT, GL_FALSE, 4, nullptr);
    glEnableVertexAttribArray(testValueLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    drawQuad(program.get(), "position", 0.5f);

    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> expected(getWindowWidth() * getWindowHeight(), GLColor::green);
    std::vector<GLColor> actual(getWindowWidth() * getWindowHeight());
    glReadPixels(0, 0, getWindowWidth(), getWindowHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 actual.data());
    EXPECT_EQ(expected, actual);
}

// Regression test for passing a zero size init buffer with the extension.
TEST_P(RobustResourceInitTest, BufferDataZeroSize)
{
    if (!setup())
    {
        return;
    }

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
}

// The following test code translated from WebGL 1 test:
// https://www.khronos.org/registry/webgl/sdk/tests/conformance/misc/uninitialized-test.html
void RobustResourceInitTest::setupTexture(GLTexture *tex)
{
    GLuint tempTexture;
    glGenTextures(1, &tempTexture);
    glBindTexture(GL_TEXTURE_2D, tempTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // this can be quite undeterministic so to improve odds of seeing uninitialized data write bits
    // into tex then delete texture then re-create one with same characteristics (driver will likely
    // reuse mem) with this trick on r59046 WebKit/OSX I get FAIL 100% of the time instead of ~15%
    // of the time.

    std::array<uint8_t, kWidth * kHeight * 4> badData;
    for (size_t i = 0; i < badData.size(); ++i)
    {
        badData[i] = static_cast<uint8_t>(i % 255);
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                    badData.data());
    glDeleteTextures(1, &tempTexture);

    // This will create the GLTexture.
    glBindTexture(GL_TEXTURE_2D, *tex);
}

void RobustResourceInitTest::setup3DTexture(GLTexture *tex)
{
    GLuint tempTexture;
    glGenTextures(1, &tempTexture);
    glBindTexture(GL_TEXTURE_3D, tempTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, kWidth, kHeight, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);

    // this can be quite undeterministic so to improve odds of seeing uninitialized data write bits
    // into tex then delete texture then re-create one with same characteristics (driver will likely
    // reuse mem) with this trick on r59046 WebKit/OSX I get FAIL 100% of the time instead of ~15%
    // of the time.

    std::array<uint8_t, kWidth * kHeight * 2 * 4> badData;
    for (size_t i = 0; i < badData.size(); ++i)
    {
        badData[i] = static_cast<uint8_t>(i % 255);
    }

    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, kWidth, kHeight, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                    badData.data());
    glDeleteTextures(1, &tempTexture);

    // This will create the GLTexture.
    glBindTexture(GL_TEXTURE_3D, *tex);
}

void RobustResourceInitTest::checkNonZeroPixels(GLTexture *texture,
                                                int skipX,
                                                int skipY,
                                                int skipWidth,
                                                int skipHeight,
                                                const GLColor &skip)
{
    glBindTexture(GL_TEXTURE_2D, 0);
    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->get(), 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    checkFramebufferNonZeroPixels(skipX, skipY, skipWidth, skipHeight, skip);
}

void RobustResourceInitTest::checkNonZeroPixels3D(GLTexture *texture,
                                                  int skipX,
                                                  int skipY,
                                                  int skipWidth,
                                                  int skipHeight,
                                                  const GLColor &skip)
{
    glBindTexture(GL_TEXTURE_3D, 0);
    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture->get(), 0, 0);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    checkFramebufferNonZeroPixels(skipX, skipY, skipWidth, skipHeight, skip);
}

void RobustResourceInitTest::checkFramebufferNonZeroPixels(int skipX,
                                                           int skipY,
                                                           int skipWidth,
                                                           int skipHeight,
                                                           const GLColor &skip)
{
    std::array<GLColor, kWidth * kHeight> data;
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

    int k = 0;
    for (int y = 0; y < kHeight; ++y)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            int index = (y * kWidth + x);
            if (x >= skipX && x < skipX + skipWidth && y >= skipY && y < skipY + skipHeight)
            {
                ASSERT_EQ(skip, data[index]);
            }
            else
            {
                k += (data[index] != GLColor::transparentBlack) ? 1 : 0;
            }
        }
    }

    EXPECT_EQ(0, k);
}

// Reading an uninitialized texture (texImage2D) should succeed with all bytes set to 0.
TEST_P(RobustResourceInitTest, ReadingUninitializedTexture)
{
    if (!setup())
    {
        return;
    }

    if (IsOpenGL() || IsD3D9())
    {
        std::cout << "Robust resource init is not yet fully implemented. (" << GetParam() << ")"
                  << std::endl;
        return;
    }

    GLTexture tex;
    setupTexture(&tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    checkNonZeroPixels(&tex, 0, 0, 0, 0, GLColor::transparentBlack);
    EXPECT_GL_NO_ERROR();
}

// Test that calling glTexImage2D multiple times with the same size and no data resets all texture
// data
TEST_P(RobustResourceInitTest, ReuploadingClearsTexture)
{
    if (!setup())
    {
        return;
    }

    if (IsOpenGL() || IsD3D9())
    {
        std::cout << "Robust resource init is not yet fully implemented. (" << GetParam() << ")"
                  << std::endl;
        return;
    }

    // Put some data into the texture
    std::array<GLColor, kWidth * kHeight> data;
    data.fill(GLColor::white);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data.data());

    // Reset the texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    checkNonZeroPixels(&tex, 0, 0, 0, 0, GLColor::transparentBlack);
    EXPECT_GL_NO_ERROR();
}

// Cover the case where null pixel data is uploaded to a texture and then sub image is used to
// upload partial data
TEST_P(RobustResourceInitTest, TexImageThenSubImage)
{
    if (!setup())
    {
        return;
    }

    if (IsOpenGL() || IsD3D9())
    {
        std::cout << "Robust resource init is not yet fully implemented. (" << GetParam() << ")"
                  << std::endl;
        return;
    }

    // Put some data into the texture

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Force the D3D texture to create a storage
    checkNonZeroPixels(&tex, 0, 0, 0, 0, GLColor::transparentBlack);

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    std::array<GLColor, kWidth * kHeight> data;
    data.fill(GLColor::white);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kWidth / 2, kHeight / 2, GL_RGBA, GL_UNSIGNED_BYTE,
                    data.data());
    checkNonZeroPixels(&tex, 0, 0, kWidth / 2, kHeight / 2, GLColor::white);
    EXPECT_GL_NO_ERROR();
}

// Reading an uninitialized texture (texImage2D) should succeed with all bytes set to 0.
TEST_P(RobustResourceInitTest, ReadingUninitialized3DTexture)
{
    if (!setup() || getClientMajorVersion() < 3)
    {
        return;
    }

    if (IsOpenGL())
    {
        std::cout << "Robust resource init is not yet fully implemented. (" << GetParam() << ")"
                  << std::endl;
        return;
    }

    GLTexture tex;
    setup3DTexture(&tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, kWidth, kHeight, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    checkNonZeroPixels3D(&tex, 0, 0, 0, 0, GLColor::transparentBlack);
    EXPECT_GL_NO_ERROR();
}

// Copy of the copytexsubimage3d_texture_wrongly_initialized test that is part of the WebGL2
// conformance suite: copy-texture-image-webgl-specific.html
TEST_P(RobustResourceInitTest, CopyTexSubImage3DTextureWronglyInitialized)
{
    if (!setup() || getClientMajorVersion() < 3)
    {
        return;
    }

    if (IsOpenGL())
    {
        std::cout << "Robust resource init is not yet fully implemented. (" << GetParam() << ")"
                  << std::endl;
        return;
    }

    constexpr GLint kTextureLayer     = 0;
    constexpr GLint kTextureWidth     = 2;
    constexpr GLint kTextureHeight    = 2;
    constexpr GLint kTextureDepth     = 2;
    constexpr size_t kTextureDataSize = kTextureWidth * kTextureHeight * 4;

    GLTexture texture2D;
    glBindTexture(GL_TEXTURE_2D, texture2D);
    constexpr std::array<uint8_t, kTextureDataSize> data = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                                             0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                                             0x0D, 0x0E, 0x0F, 0x10}};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTextureWidth, kTextureHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data.data());

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture2D, 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GLTexture texture3D;
    glBindTexture(GL_TEXTURE_3D, texture3D);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, kTextureWidth, kTextureHeight, kTextureDepth);
    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, kTextureLayer, 0, 0, kTextureWidth, kTextureHeight);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture3D, 0, kTextureLayer);
    std::array<uint8_t, kTextureDataSize> pixels;
    glReadPixels(0, 0, kTextureWidth, kTextureHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(data, pixels);
}

// Test that binding an EGL surface to a texture does not cause it to be cleared.
TEST_P(RobustResourceInitTest, BindTexImage)
{
    if (!setup() || getClientMajorVersion() < 3)
    {
        return;
    }

    if (IsOpenGL())
    {
        std::cout << "Robust resource init is not yet fully implemented. (" << GetParam() << ")"
                  << std::endl;
        return;
    }

    EGLWindow *window  = getEGLWindow();
    EGLSurface surface = window->getSurface();
    EGLDisplay display = window->getDisplay();
    EGLConfig config   = window->getConfig();
    EGLContext context = window->getContext();

    EGLint surfaceType = 0;
    eglGetConfigAttrib(display, config, EGL_SURFACE_TYPE, &surfaceType);
    if ((surfaceType & EGL_PBUFFER_BIT) == 0)
    {
        std::cout << "Test skipped because EGL config cannot be used to create pbuffers."
                  << std::endl;
        return;
    }

    EGLint attribs[] = {
        EGL_WIDTH,          32,
        EGL_HEIGHT,         32,
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
        EGL_NONE,
    };

    EGLSurface pbuffer = eglCreatePbufferSurface(display, config, attribs);
    ASSERT_NE(EGL_NO_SURFACE, pbuffer);

    // Clear the pbuffer
    eglMakeCurrent(display, pbuffer, pbuffer, context);
    GLColor clearColor = GLColor::magenta;
    glClearColor(clearColor.R, clearColor.G, clearColor.B, clearColor.A);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, clearColor);

    // Bind the pbuffer to a texture and read its color
    eglMakeCurrent(display, surface, surface, context);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    eglBindTexImage(display, pbuffer, EGL_BACK_BUFFER);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, clearColor);

    eglDestroySurface(display, pbuffer);
}

ANGLE_INSTANTIATE_TEST(RobustResourceInitTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());
}  // namespace
