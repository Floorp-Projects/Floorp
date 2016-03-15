//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// StateChangeTest:
//   Specifically designed for an ANGLE implementation of GL, these tests validate that
//   ANGLE's dirty bits systems don't get confused by certain sequences of state changes.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

class StateChangeTest : public ANGLETest
{
  protected:
    StateChangeTest() : mFramebuffer(0)
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);

        // Enable the no error extension to avoid syncing the FBO state on validation.
        setNoErrorEnabled(true);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenFramebuffers(1, &mFramebuffer);

        mTextures.resize(2, 0);
        glGenTextures(2, mTextures.data());

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        if (mFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &mFramebuffer);
            mFramebuffer = 0;
        }

        if (!mTextures.empty())
        {
            glDeleteTextures(static_cast<GLsizei>(mTextures.size()), mTextures.data());
            mTextures.clear();
        }

        ANGLETest::TearDown();
    }

    GLuint mFramebuffer;
    std::vector<GLuint> mTextures;
};

class StateChangeTestES3 : public StateChangeTest
{
  protected:
    StateChangeTestES3() {}
};

}  // anonymous namespace

// Ensure that CopyTexImage2D syncs framebuffer changes.
TEST_P(StateChangeTest, CopyTexImage2DSync)
{
    if (isAMD() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        // TODO(geofflang): Fix on Linux AMD drivers (http://anglebug.com/1291)
        std::cout << "Test disabled on AMD OpenGL." << std::endl;
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Init first texture to red
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    // Init second texture to green
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    // Copy in the red texture to the green one.
    // CopyTexImage should sync the framebuffer attachment change.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 16, 16, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

// Ensure that CopyTexSubImage2D syncs framebuffer changes.
TEST_P(StateChangeTest, CopyTexSubImage2DSync)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Init first texture to red
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    // Init second texture to green
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    // Copy in the red texture to the green one.
    // CopyTexImage should sync the framebuffer attachment change.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 16, 16);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

// Ensure that CopyTexSubImage3D syncs framebuffer changes.
TEST_P(StateChangeTestES3, CopyTexSubImage3DSync)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Init first texture to red
    glBindTexture(GL_TEXTURE_3D, mTextures[0]);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 16, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextures[0], 0, 0);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    // Init second texture to green
    glBindTexture(GL_TEXTURE_3D, mTextures[1]);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 16, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextures[1], 0, 0);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    // Copy in the red texture to the green one.
    // CopyTexImage should sync the framebuffer attachment change.
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextures[0], 0, 0);
    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 16, 16);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextures[1], 0, 0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

// Ensure that BlitFramebuffer syncs framebuffer changes.
TEST_P(StateChangeTestES3, BlitFramebufferSync)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Init first texture to red
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    // Init second texture to green
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1], 0);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    // Change to the red textures and blit.
    // BlitFramebuffer should sync the framebuffer attachment change.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0],
                           0);
    glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

// Ensure that ReadBuffer and DrawBuffers sync framebuffer changes.
TEST_P(StateChangeTestES3, ReadBufferAndDrawBuffersSync)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    // Initialize two FBO attachments
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mTextures[1], 0);

    // Clear first attachment to red
    GLenum bufs1[] = {GL_COLOR_ATTACHMENT0, GL_NONE};
    glDrawBuffers(2, bufs1);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Clear second texture to green
    GLenum bufs2[] = {GL_NONE, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs2);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Verify first attachment is red and second is green
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);

    ASSERT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(StateChangeTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL());
ANGLE_INSTANTIATE_TEST(StateChangeTestES3, ES3_D3D11(), ES3_OPENGL());
