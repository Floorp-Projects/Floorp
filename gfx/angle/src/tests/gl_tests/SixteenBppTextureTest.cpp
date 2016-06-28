//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SixteenBppTextureTest:
//   Basic tests using 16bpp texture formats (e.g. GL_RGB565).

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class SixteenBppTextureTest : public ANGLETest
{
  protected:
    SixteenBppTextureTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        const std::string vertexShaderSource = SHADER_SOURCE
        (
            precision highp float;
            attribute vec4 position;
            varying vec2 texcoord;

            void main()
            {
                gl_Position = vec4(position.xy, 0.0, 1.0);
                texcoord = (position.xy * 0.5) + 0.5;
            }
        );

        const std::string fragmentShaderSource2D = SHADER_SOURCE
        (
            precision highp float;
            uniform sampler2D tex;
            varying vec2 texcoord;

            void main()
            {
                gl_FragColor = texture2D(tex, texcoord);
            }
        );

        m2DProgram = CompileProgram(vertexShaderSource, fragmentShaderSource2D);
        mTexture2DUniformLocation = glGetUniformLocation(m2DProgram, "tex");
    }

    void TearDown() override
    {
        glDeleteProgram(m2DProgram);

        ANGLETest::TearDown();
    }

    void simpleValidationBase(GLuint tex)
    {
        // Draw a quad using the texture
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(m2DProgram);
        glUniform1i(mTexture2DUniformLocation, 0);
        drawQuad(m2DProgram, "position", 0.5f);
        ASSERT_GL_NO_ERROR();

        int w = getWindowWidth() - 1;
        int h = getWindowHeight() - 1;

        // Check that it drew as expected
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(w, 0, GLColor::green);
        EXPECT_PIXEL_COLOR_EQ(0, h, GLColor::blue);
        EXPECT_PIXEL_COLOR_EQ(w, h, GLColor::yellow);

        // Generate mipmaps
        glGenerateMipmap(GL_TEXTURE_2D);

        // Draw a quad using the texture
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(m2DProgram);
        glUniform1i(mTexture2DUniformLocation, 0);
        drawQuad(m2DProgram, "position", 0.5f);
        ASSERT_GL_NO_ERROR();

        // Check that it drew as expected
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(w, 0, GLColor::green);
        EXPECT_PIXEL_COLOR_EQ(0, h, GLColor::blue);
        EXPECT_PIXEL_COLOR_EQ(w, h, GLColor::yellow);

        // Bind the texture as a framebuffer, render to it, then check the results
        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_UNSUPPORTED)
        {
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);
            EXPECT_PIXEL_EQ(1, 0, 255, 0, 0, 255);
            EXPECT_PIXEL_EQ(1, 1, 255, 0, 0, 255);
            EXPECT_PIXEL_EQ(0, 1, 255, 0, 0, 255);
        }
        else
        {
            std::cout << "Skipping rendering to an unsupported framebuffer format" << std::endl;
        }
    }

    GLuint m2DProgram;
    GLint mTexture2DUniformLocation;
};

class SixteenBppTextureTestES3 : public SixteenBppTextureTest
{
};

// Simple validation test for GL_RGB565 textures.
// Samples from the texture, renders to it, generates mipmaps etc.
TEST_P(SixteenBppTextureTest, RGB565Validation)
{
    // These tests fail on certain Intel machines running an un-updated version of Win7
    // The tests pass after installing the latest updates from Windows Update.
    // TODO: reenable these tests once the bots have been updated
    if (IsIntel() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        std::cout << "Test skipped on Intel D3D11." << std::endl;
        return;
    }

    GLushort pixels[4] =
    {
        0xF800, // Red
        0x07E0, // Green
        0x001F, // Blue
        0xFFE0  // Red + Green
    };

    glClearColor(0, 0, 0, 0);

    // Create a simple RGB565 texture
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    // Supply the data to it
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
    EXPECT_GL_NO_ERROR();

    simpleValidationBase(tex.get());
}

// Simple validation test for GL_RGBA5551 textures.
// Samples from the texture, renders to it, generates mipmaps etc.
TEST_P(SixteenBppTextureTest, RGBA5551Validation)
{
    // These tests fail on certain Intel machines running an un-updated version of Win7
    // The tests pass after installing the latest updates from Windows Update.
    // TODO: reenable these tests once the bots have been updated
    if (IsIntel() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        std::cout << "Test skipped on Intel D3D11." << std::endl;
        return;
    }

    GLushort pixels[4] =
    {
        0xF801, // Red
        0x07C1, // Green
        0x003F, // Blue
        0xFFC1  // Red + Green
    };

    // Create a simple 5551 texture
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, pixels);
    EXPECT_GL_NO_ERROR();

    simpleValidationBase(tex.get());
}

// Test to ensure calling Clear() on an RGBA5551 texture does something reasonable
// Based on WebGL test conformance/textures/texture-attachment-formats.html
TEST_P(SixteenBppTextureTest, RGBA5551ClearAlpha)
{
    // These tests fail on certain Intel machines running an un-updated version of Win7
    // The tests pass after installing the latest updates from Windows Update.
    // TODO: reenable these tests once the bots have been updated
    if (IsIntel() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        std::cout << "Test skipped on Intel D3D11." << std::endl;
        return;
    }

    GLTexture tex;
    GLFramebuffer fbo;

    // Create a simple 5551 texture
    glBindTexture(GL_TEXTURE_2D, tex.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    // Bind the texture as a framebuffer, clear it, then check the results
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.get(), 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_UNSUPPORTED)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_EQ(0, 0, 0, 0, 0, 0);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_EQ(0, 0, 0, 0, 0, 255);
    }
    else
    {
        std::cout << "Skipping rendering to an unsupported framebuffer format" << std::endl;
    }
}

// Simple validation test for GL_RGBA4444 textures.
// Samples from the texture, renders to it, generates mipmaps etc.
TEST_P(SixteenBppTextureTest, RGBA4444Validation)
{
    // These tests fail on certain Intel machines running an un-updated version of Win7
    // The tests pass after installing the latest updates from Windows Update.
    // TODO: reenable these tests once the bots have been updated
    if (IsIntel() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        std::cout << "Test skipped on Intel D3D11." << std::endl;
        return;
    }

    GLushort pixels[4] =
    {
        0xF00F, // Red
        0x0F0F, // Green
        0x00FF, // Blue
        0xFF0F  // Red + Green
    };

    glClearColor(0, 0, 0, 0);

    // Generate a RGBA4444 texture, no mipmaps
    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    // Provide some data for the texture
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, pixels);
    EXPECT_GL_NO_ERROR();

    simpleValidationBase(tex.get());
}

// Test uploading RGBA8 data to RGBA4 textures.
TEST_P(SixteenBppTextureTestES3, RGBA4UploadRGBA8)
{
    std::vector<GLColor> fourColors;
    fourColors.push_back(GLColor::red);
    fourColors.push_back(GLColor::green);
    fourColors.push_back(GLColor::blue);
    fourColors.push_back(GLColor::yellow);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, fourColors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    simpleValidationBase(tex.get());
}

// Test uploading RGB8 data to RGB565 textures.
TEST_P(SixteenBppTextureTestES3, RGB565UploadRGB8)
{
    std::vector<GLColorRGB> fourColors;
    fourColors.push_back(GLColorRGB::red);
    fourColors.push_back(GLColorRGB::green);
    fourColors.push_back(GLColorRGB::blue);
    fourColors.push_back(GLColorRGB::yellow);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, fourColors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    simpleValidationBase(tex.get());
}

// Test uploading RGBA8 data to RGB5A41 textures.
TEST_P(SixteenBppTextureTestES3, RGB5A1UploadRGBA8)
{
    std::vector<GLColor> fourColors;
    fourColors.push_back(GLColor::red);
    fourColors.push_back(GLColor::green);
    fourColors.push_back(GLColor::blue);
    fourColors.push_back(GLColor::yellow);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 fourColors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    simpleValidationBase(tex.get());
}

// Test uploading RGB10A2 data to RGB5A1 textures.
TEST_P(SixteenBppTextureTestES3, RGB5A1UploadRGB10A2)
{
    struct RGB10A2
    {
        RGB10A2(uint32_t r, uint32_t g, uint32_t b, uint32_t a) : R(r), G(g), B(b), A(a) {}

        uint32_t R : 10;
        uint32_t G : 10;
        uint32_t B : 10;
        uint32_t A : 2;
    };

    uint32_t one10 = (1u << 10u) - 1u;

    RGB10A2 red(one10, 0u, 0u, 0x3u);
    RGB10A2 green(0u, one10, 0u, 0x3u);
    RGB10A2 blue(0u, 0u, one10, 0x3u);
    RGB10A2 yellow(one10, one10, 0u, 0x3u);

    std::vector<RGB10A2> fourColors;
    fourColors.push_back(red);
    fourColors.push_back(green);
    fourColors.push_back(blue);
    fourColors.push_back(yellow);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 2, 2, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV,
                 fourColors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    simpleValidationBase(tex.get());
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(SixteenBppTextureTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(),
                       ES2_OPENGLES());

ANGLE_INSTANTIATE_TEST(SixteenBppTextureTestES3, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());

} // namespace
