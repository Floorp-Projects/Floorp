//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WebGLCompatibilityTest.cpp : Tests of the GL_ANGLE_webgl_compatibility extension.

#include "test_utils/ANGLETest.h"

#include "common/mathutil.h"
#include "test_utils/gl_raii.h"

namespace
{

bool ConstantColorAndAlphaBlendFunctions(GLenum first, GLenum second)
{
    return (first == GL_CONSTANT_COLOR || first == GL_ONE_MINUS_CONSTANT_COLOR) &&
           (second == GL_CONSTANT_ALPHA || second == GL_ONE_MINUS_CONSTANT_ALPHA);
}

void CheckBlendFunctions(GLenum src, GLenum dst)
{
    if (ConstantColorAndAlphaBlendFunctions(src, dst) ||
        ConstantColorAndAlphaBlendFunctions(dst, src))
    {
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
    else
    {
        ASSERT_GL_NO_ERROR();
    }
}

// Extensions that affect the ability to use floating point textures
constexpr const char *FloatingPointTextureExtensions[] = {
    "",
    "GL_EXT_texture_storage",
    "GL_OES_texture_half_float",
    "GL_OES_texture_half_float_linear",
    "GL_EXT_color_buffer_half_float",
    "GL_OES_texture_float",
    "GL_OES_texture_float_linear",
    "GL_EXT_color_buffer_float",
    "GL_CHROMIUM_color_buffer_float_rgba",
    "GL_CHROMIUM_color_buffer_float_rgb",
};

}  // namespace

namespace angle
{

class WebGLCompatibilityTest : public ANGLETest
{
  protected:
    WebGLCompatibilityTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setWebGLCompatibilityEnabled(true);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();
        glRequestExtensionANGLE = reinterpret_cast<PFNGLREQUESTEXTENSIONANGLEPROC>(
            eglGetProcAddress("glRequestExtensionANGLE"));
    }

    template <typename T>
    void TestFloatTextureFormat(GLenum internalFormat,
                                GLenum format,
                                GLenum type,
                                bool texturingEnabled,
                                bool linearSamplingEnabled,
                                bool renderingEnabled,
                                const T textureData[4],
                                const float floatData[4])
    {
        ASSERT_GL_NO_ERROR();

        const std::string samplingVs =
            "attribute vec4 position;\n"
            "varying vec2 texcoord;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
            "    texcoord = (position.xy * 0.5) + 0.5;\n"
            "}\n";

        const std::string samplingFs =
            "precision mediump float;\n"
            "uniform sampler2D tex;\n"
            "uniform vec4 subtractor;\n"
            "varying vec2 texcoord;\n"
            "void main()\n"
            "{\n"
            "    vec4 color = texture2D(tex, texcoord);\n"
            "    if (abs(color.r - subtractor.r) +\n"
            "        abs(color.g - subtractor.g) +\n"
            "        abs(color.b - subtractor.b) +\n"
            "        abs(color.a - subtractor.a) < 8.0)\n"
            "    {\n"
            "        gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
            "    }\n"
            "    else\n"
            "    {\n"
            "        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
            "    }\n"
            "}\n";

        ANGLE_GL_PROGRAM(samplingProgram, samplingVs, samplingFs);
        glUseProgram(samplingProgram.get());

        GLRenderbuffer rbo;
        glBindRenderbuffer(GL_RENDERBUFFER, rbo.get());
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);

        GLFramebuffer fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo.get());

        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture.get());

        if (internalFormat == format)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 1, 1, 0, format, type, textureData);
        }
        else
        {
            if (getClientMajorVersion() >= 3)
            {
                glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, 1, 1);
            }
            else
            {
                ASSERT_TRUE(extensionEnabled("GL_EXT_texture_storage"));
                glTexStorage2DEXT(GL_TEXTURE_2D, 1, internalFormat, 1, 1);
            }
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, format, type, textureData);
        }

        if (!texturingEnabled)
        {
            // Depending on the entry point and client version, different errors may be generated
            ASSERT_GLENUM_NE(GL_NO_ERROR, glGetError());

            // Two errors may be generated in the glTexStorage + glTexSubImage case, clear the
            // second error
            glGetError();

            return;
        }
        ASSERT_GL_NO_ERROR();

        glUniform1i(glGetUniformLocation(samplingProgram.get(), "tex"), 0);
        glUniform4fv(glGetUniformLocation(samplingProgram.get(), "subtractor"), 1, floatData);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        drawQuad(samplingProgram.get(), "position", 0.5f, 1.0f, true);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        drawQuad(samplingProgram.get(), "position", 0.5f, 1.0f, true);

        if (linearSamplingEnabled)
        {
            EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        }
        else
        {
            EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(),
                               0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (!renderingEnabled)
        {
            EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                             glCheckFramebufferStatus(GL_FRAMEBUFFER));
            return;
        }
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

        const std::string renderingVs =
            "attribute vec4 position;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vec4(position.xy, 0.0, 1.0);\n"
            "}\n";

        const std::string renderingFs =
            "precision mediump float;\n"
            "uniform vec4 writeValue;\n"
            "void main()\n"
            "{\n"
            "   gl_FragColor = writeValue;\n"
            "}\n";

        ANGLE_GL_PROGRAM(renderingProgram, renderingVs, renderingFs);
        glUseProgram(renderingProgram.get());

        glUniform4fv(glGetUniformLocation(renderingProgram.get(), "writeValue"), 1, floatData);

        drawQuad(renderingProgram.get(), "position", 0.5f, 1.0f, true);

        EXPECT_PIXEL_COLOR32F_NEAR(
            0, 0, GLColor32F(floatData[0], floatData[1], floatData[2], floatData[3]), 1.0f);
    }

    // Called from RenderingFeedbackLoopWithDrawBuffersEXT.
    void drawBuffersEXTFeedbackLoop(GLuint program,
                                    const std::array<GLenum, 2> &drawBuffers,
                                    GLenum expectedError);

    // Called from RenderingFeedbackLoopWithDrawBuffers.
    void drawBuffersFeedbackLoop(GLuint program,
                                 const std::array<GLenum, 2> &drawBuffers,
                                 GLenum expectedError);

    PFNGLREQUESTEXTENSIONANGLEPROC glRequestExtensionANGLE = nullptr;
};

class WebGL2CompatibilityTest : public WebGLCompatibilityTest
{
};

// Context creation would fail if EGL_ANGLE_create_context_webgl_compatibility was not available so
// the GL extension should always be present
TEST_P(WebGLCompatibilityTest, ExtensionStringExposed)
{
    EXPECT_TRUE(extensionEnabled("GL_ANGLE_webgl_compatibility"));
}

// Verify that all extension entry points are available
TEST_P(WebGLCompatibilityTest, EntryPoints)
{
    if (extensionEnabled("GL_ANGLE_request_extension"))
    {
        EXPECT_NE(nullptr, eglGetProcAddress("glRequestExtensionANGLE"));
    }
}

// WebGL 1 allows GL_DEPTH_STENCIL_ATTACHMENT as a valid binding point.  Make sure it is usable,
// even in ES2 contexts.
TEST_P(WebGLCompatibilityTest, DepthStencilBindingPoint)
{
    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 32, 32);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffer.get());

    EXPECT_GL_NO_ERROR();
}

// Test that attempting to enable an extension that doesn't exist generates GL_INVALID_OPERATION
TEST_P(WebGLCompatibilityTest, EnableExtensionValidation)
{
    glRequestExtensionANGLE("invalid_extension_string");
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test enabling the GL_OES_element_index_uint extension
TEST_P(WebGLCompatibilityTest, EnableExtensionUintIndices)
{
    if (getClientMajorVersion() != 2)
    {
        // This test only works on ES2 where uint indices are not available by default
        return;
    }

    EXPECT_FALSE(extensionEnabled("GL_OES_element_index_uint"));

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get());

    GLuint data[] = {0, 1, 2, 1, 3, 2};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

    ANGLE_GL_PROGRAM(program, "void main() { gl_Position = vec4(0, 0, 0, 1); }",
                     "void main() { gl_FragColor = vec4(0, 1, 0, 1); }")
    glUseProgram(program.get());

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (extensionRequestable("GL_OES_element_index_uint"))
    {
        glRequestExtensionANGLE("GL_OES_element_index_uint");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(extensionEnabled("GL_OES_element_index_uint"));

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        EXPECT_GL_NO_ERROR();
    }
}

// Test enabling the GL_OES_standard_derivatives extension
TEST_P(WebGLCompatibilityTest, EnableExtensionStandardDerivitives)
{
    EXPECT_FALSE(extensionEnabled("GL_OES_standard_derivatives"));

    const std::string source =
        "#extension GL_OES_standard_derivatives : require\n"
        "void main() { gl_FragColor = vec4(dFdx(vec2(1.0, 1.0)).x, 1, 0, 1); }\n";
    ASSERT_EQ(0u, CompileShader(GL_FRAGMENT_SHADER, source));

    if (extensionRequestable("GL_OES_standard_derivatives"))
    {
        glRequestExtensionANGLE("GL_OES_standard_derivatives");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(extensionEnabled("GL_OES_standard_derivatives"));

        GLuint shader = CompileShader(GL_FRAGMENT_SHADER, source);
        ASSERT_NE(0u, shader);
        glDeleteShader(shader);
    }
}

// Test enabling the GL_EXT_shader_texture_lod extension
TEST_P(WebGLCompatibilityTest, EnableExtensionTextureLOD)
{
    EXPECT_FALSE(extensionEnabled("GL_EXT_shader_texture_lod"));

    const std::string source =
        "#extension GL_EXT_shader_texture_lod : require\n"
        "uniform sampler2D u_texture;\n"
        "void main() {\n"
        "    gl_FragColor = texture2DGradEXT(u_texture, vec2(0.0, 0.0), vec2(0.0, 0.0), vec2(0.0, "
        "0.0));\n"
        "}\n";
    ASSERT_EQ(0u, CompileShader(GL_FRAGMENT_SHADER, source));

    if (extensionRequestable("GL_EXT_shader_texture_lod"))
    {
        glRequestExtensionANGLE("GL_EXT_shader_texture_lod");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(extensionEnabled("GL_EXT_shader_texture_lod"));

        GLuint shader = CompileShader(GL_FRAGMENT_SHADER, source);
        ASSERT_NE(0u, shader);
        glDeleteShader(shader);
    }
}

// Test enabling the GL_EXT_frag_depth extension
TEST_P(WebGLCompatibilityTest, EnableExtensionFragDepth)
{
    EXPECT_FALSE(extensionEnabled("GL_EXT_frag_depth"));

    const std::string source =
        "#extension GL_EXT_frag_depth : require\n"
        "void main() {\n"
        "    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "    gl_FragDepthEXT = 1.0;\n"
        "}\n";
    ASSERT_EQ(0u, CompileShader(GL_FRAGMENT_SHADER, source));

    if (extensionRequestable("GL_EXT_frag_depth"))
    {
        glRequestExtensionANGLE("GL_EXT_frag_depth");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(extensionEnabled("GL_EXT_frag_depth"));

        GLuint shader = CompileShader(GL_FRAGMENT_SHADER, source);
        ASSERT_NE(0u, shader);
        glDeleteShader(shader);
    }
}

// Test enabling the GL_EXT_texture_filter_anisotropic extension
TEST_P(WebGLCompatibilityTest, EnableExtensionTextureFilterAnisotropic)
{
    EXPECT_FALSE(extensionEnabled("GL_EXT_texture_filter_anisotropic"));

    GLfloat maxAnisotropy = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    ASSERT_GL_NO_ERROR();

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    GLfloat currentAnisotropy = 0.0f;
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &currentAnisotropy);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    if (extensionRequestable("GL_EXT_texture_filter_anisotropic"))
    {
        glRequestExtensionANGLE("GL_EXT_texture_filter_anisotropic");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(extensionEnabled("GL_EXT_texture_filter_anisotropic"));

        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
        ASSERT_GL_NO_ERROR();
        EXPECT_GE(maxAnisotropy, 2.0f);

        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &currentAnisotropy);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(1.0f, currentAnisotropy);

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
        ASSERT_GL_NO_ERROR();
    }
}

// Verify that shaders are of a compatible spec when the extension is enabled.
TEST_P(WebGLCompatibilityTest, ExtensionCompilerSpec)
{
    EXPECT_TRUE(extensionEnabled("GL_ANGLE_webgl_compatibility"));

    // Use of reserved _webgl prefix should fail when the shader specification is for WebGL.
    const std::string &vert =
        "struct Foo {\n"
        "    int _webgl_bar;\n"
        "};\n"
        "void main()\n"
        "{\n"
        "    Foo foo = Foo(1);\n"
        "}";

    // Default fragement shader.
    const std::string &frag =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0,0.0,0.0,1.0);\n"
        "}";

    GLuint program = CompileProgram(vert, frag);
    EXPECT_EQ(0u, program);
    glDeleteProgram(program);
}

// Verify that the context generates the correct error when the framebuffer attachments are
// different sizes
TEST_P(WebGLCompatibilityTest, FramebufferAttachmentSizeMissmatch)
{
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLTexture textures[2];
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);

    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 3, 3);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);

    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    if (extensionRequestable("GL_EXT_draw_buffers"))
    {
        glRequestExtensionANGLE("GL_EXT_draw_buffers");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(extensionEnabled("GL_EXT_draw_buffers"));

        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textures[1], 0);
        ASSERT_GL_NO_ERROR();

        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);

        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        ASSERT_GL_NO_ERROR();
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS,
                         glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }
}

// Test that client-side array buffers are forbidden in WebGL mode
TEST_P(WebGLCompatibilityTest, ForbidsClientSideArrayBuffer)
{
    const std::string &vert =
        "attribute vec3 a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_pos, 1.0);\n"
        "}\n";

    const std::string &frag =
        "precision highp float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vert, frag);

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    const auto &vertices = GetQuadVertices();
    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 4, vertices.data());
    glEnableVertexAttribArray(posLocation);

    ASSERT_GL_NO_ERROR();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that client-side element array buffers are forbidden in WebGL mode
TEST_P(WebGLCompatibilityTest, ForbidsClientSideElementBuffer)
{
    const std::string &vert =
        "attribute vec3 a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_pos, 1.0);\n"
        "}\n";

    const std::string &frag =
        "precision highp float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vert, frag);

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    const auto &vertices = GetQuadVertices();

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get());
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    ASSERT_GL_NO_ERROR();

    // Use the pointer with value of 1 for indices instead of an actual pointer because WebGL also
    // enforces that the top bit of indices must be 0 (i.e. offset >= 0) and would generate
    // GL_INVALID_VALUE in that case. Using a null pointer gets caught by another check.
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, reinterpret_cast<const void*>(intptr_t(1)));
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that client-side array buffers are forbidden even if the program doesn't use the attribute
TEST_P(WebGLCompatibilityTest, ForbidsClientSideArrayBufferEvenNotUsedOnes)
{
    const std::string &vert =
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(1.0);\n"
        "}\n";

    const std::string &frag =
        "precision highp float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vert, frag);

    glUseProgram(program.get());

    const auto &vertices = GetQuadVertices();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4, vertices.data());
    glEnableVertexAttribArray(0);

    ASSERT_GL_NO_ERROR();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Tests the WebGL requirement of having the same stencil mask, writemask and ref for fron and back
TEST_P(WebGLCompatibilityTest, RequiresSameStencilMaskAndRef)
{
    // Run the test in an FBO to make sure we have some stencil bits.
    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 32, 32);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffer.get());

    ANGLE_GL_PROGRAM(program, "void main() { gl_Position = vec4(0, 0, 0, 1); }",
                     "void main() { gl_FragColor = vec4(0, 1, 0, 1); }")
    glUseProgram(program.get());
    ASSERT_GL_NO_ERROR();

    // Having ref and mask the same for front and back is valid.
    glStencilMask(255);
    glStencilFunc(GL_ALWAYS, 0, 255);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();

    // Having a different front - back write mask generates an error.
    glStencilMaskSeparate(GL_FRONT, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Setting both write masks separately to the same value is valid.
    glStencilMaskSeparate(GL_BACK, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();

    // Having a different stencil front - back mask generates an error
    glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Setting both masks separately to the same value is valid.
    glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();

    // Having a different stencil front - back reference generates an error
    glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 255, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Setting both references separately to the same value is valid.
    glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 255, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();

    // Using different stencil funcs, everything being equal is valid.
    glStencilFuncSeparate(GL_BACK, GL_NEVER, 255, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();
}

// Test that GL_FIXED is forbidden
TEST_P(WebGLCompatibilityTest, ForbidsGLFixed)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    ASSERT_GL_NO_ERROR();

    glVertexAttribPointer(0, 1, GL_FIXED, GL_FALSE, 0, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Test the WebGL limit of 255 for the attribute stride
TEST_P(WebGLCompatibilityTest, MaxStride)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 255, nullptr);
    ASSERT_GL_NO_ERROR();

    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 256, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test the checks for OOB reads in the vertex buffers, non-instanced version
TEST_P(WebGLCompatibilityTest, DrawArraysBufferOutOfBoundsNonInstanced)
{
    const std::string &vert =
        "attribute float a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_pos, a_pos, a_pos, 1.0);\n"
        "}\n";

    const std::string &frag =
        "precision highp float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vert, frag);

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);

    const uint8_t* zeroOffset = nullptr;

    // Test touching the last element is valid.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 12);
    glDrawArrays(GL_POINTS, 0, 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last element + 1 is invalid.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 13);
    glDrawArrays(GL_POINTS, 0, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test touching the last element is valid, using a stride.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 9);
    glDrawArrays(GL_POINTS, 0, 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last element + 1 is invalid, using a stride.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 10);
    glDrawArrays(GL_POINTS, 0, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test any offset is valid if no vertices are drawn.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 32);
    glDrawArrays(GL_POINTS, 0, 0);
    ASSERT_GL_NO_ERROR();
}

// Test the checks for OOB reads in the index buffer
TEST_P(WebGLCompatibilityTest, DrawElementsBufferOutOfBoundsInIndexBuffer)
{
    const std::string &vert =
        "attribute float a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_pos, a_pos, a_pos, 1.0);\n"
        "}\n";

    const std::string &frag =
        "precision highp float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vert, frag);

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);

    const uint8_t *zeroOffset   = nullptr;
    const uint8_t zeroIndices[] = {0, 0, 0, 0, 0, 0, 0, 0};

    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset);

    GLBuffer indexBuffer;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(zeroIndices), zeroIndices, GL_STATIC_DRAW);
    ASSERT_GL_NO_ERROR();

    // Test touching the last index is valid
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, zeroOffset + 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last + 1 element is invalid
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, zeroOffset + 5);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test any offset if valid if count is zero
    glDrawElements(GL_POINTS, 0, GL_UNSIGNED_BYTE, zeroOffset + 42);
    ASSERT_GL_NO_ERROR();

    // Test touching the first index is valid
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, zeroOffset + 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the first - 1 index is invalid
    // The error ha been specified to be INVALID_VALUE instead of INVALID_OPERATION because it was
    // the historic behavior of WebGL implementations
    glDrawElements(GL_POINTS, 4, GL_UNSIGNED_BYTE, zeroOffset - 1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test depth range with 'near' more or less than 'far.'
TEST_P(WebGLCompatibilityTest, DepthRange)
{
    glDepthRangef(0, 1);
    ASSERT_GL_NO_ERROR();

    glDepthRangef(.5, .5);
    ASSERT_GL_NO_ERROR();

    glDepthRangef(1, 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test all blend function combinations.
// In WebGL it is invalid to combine constant color with constant alpha.
TEST_P(WebGLCompatibilityTest, BlendWithConstantColor)
{
    constexpr GLenum srcFunc[] = {
        GL_ZERO,
        GL_ONE,
        GL_SRC_COLOR,
        GL_ONE_MINUS_SRC_COLOR,
        GL_DST_COLOR,
        GL_ONE_MINUS_DST_COLOR,
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA,
        GL_DST_ALPHA,
        GL_ONE_MINUS_DST_ALPHA,
        GL_CONSTANT_COLOR,
        GL_ONE_MINUS_CONSTANT_COLOR,
        GL_CONSTANT_ALPHA,
        GL_ONE_MINUS_CONSTANT_ALPHA,
        GL_SRC_ALPHA_SATURATE,
    };

    constexpr GLenum dstFunc[] = {
        GL_ZERO,           GL_ONE,
        GL_SRC_COLOR,      GL_ONE_MINUS_SRC_COLOR,
        GL_DST_COLOR,      GL_ONE_MINUS_DST_COLOR,
        GL_SRC_ALPHA,      GL_ONE_MINUS_SRC_ALPHA,
        GL_DST_ALPHA,      GL_ONE_MINUS_DST_ALPHA,
        GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR,
        GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA,
    };

    for (GLenum src : srcFunc)
    {
        for (GLenum dst : dstFunc)
        {
            glBlendFunc(src, dst);
            CheckBlendFunctions(src, dst);
            glBlendFuncSeparate(src, dst, GL_ONE, GL_ONE);
            CheckBlendFunctions(src, dst);
        }
    }
}

// Test that binding/querying uniforms and attributes with invalid names generates errors
TEST_P(WebGLCompatibilityTest, InvalidAttributeAndUniformNames)
{
    const std::string validAttribName =
        "abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
    const std::string validUniformName =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_1234567890";
    std::vector<char> invalidSet = {'"', '$', '`', '@', '\''};
    if (getClientMajorVersion() < 3)
    {
        invalidSet.push_back('\\');
    }

    std::string vert = "attribute float ";
    vert += validAttribName;
    vert +=
        ";\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(1.0);\n"
        "}\n";

    std::string frag =
        "precision highp float;\n"
        "uniform vec4 ";
    frag += validUniformName;
    // Insert illegal characters into comments
    frag +=
        ";\n"
        "    // $ \" @ /*\n"
        "void main()\n"
        "{/*\n"
        "    ` @ $\n"
        "    */gl_FragColor = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vert, frag);
    EXPECT_GL_NO_ERROR();

    for (char invalidChar : invalidSet)
    {
        std::string invalidName = validAttribName + invalidChar;
        glGetAttribLocation(program, invalidName.c_str());
        EXPECT_GL_ERROR(GL_INVALID_VALUE)
            << "glGetAttribLocation unexpectedly succeeded for name \"" << invalidName << "\".";

        glBindAttribLocation(program, 0, invalidName.c_str());
        EXPECT_GL_ERROR(GL_INVALID_VALUE)
            << "glBindAttribLocation unexpectedly succeeded for name \"" << invalidName << "\".";
    }

    for (char invalidChar : invalidSet)
    {
        std::string invalidName = validUniformName + invalidChar;
        glGetUniformLocation(program, invalidName.c_str());
        EXPECT_GL_ERROR(GL_INVALID_VALUE)
            << "glGetUniformLocation unexpectedly succeeded for name \"" << invalidName << "\".";
    }

    for (char invalidChar : invalidSet)
    {
        std::string invalidAttribName = validAttribName + invalidChar;
        const char *invalidVert[] = {
            "attribute float ",
            invalidAttribName.c_str(),
            ";\n",
            "void main()\n",
            "{\n",
            "    gl_Position = vec4(1.0);\n",
            "}\n",
        };

        GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(shader, static_cast<GLsizei>(ArraySize(invalidVert)), invalidVert, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
        glDeleteShader(shader);
    }
}

// Test that line continuation is handled correctly when valdiating shader source
TEST_P(WebGL2CompatibilityTest, ShaderSourceLineContinuation)
{
    const char *validVert =
        "#version 300 es\n"
        "precision mediump float;\n"
        "\n"
        "void main ()\n"
        "{\n"
        "    float f\\\n"
        "oo = 1.0;\n"
        "    gl_Position = vec4(foo);\n"
        "}\n";

    const char *invalidVert =
        "#version 300 es\n"
        "precision mediump float;\n"
        "\n"
        "void main ()\n"
        "{\n"
        "    float f\\$\n"
        "oo = 1.0;\n"
        "    gl_Position = vec4(foo);\n"
        "}\n";

    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader, 1, &validVert, nullptr);
    EXPECT_GL_NO_ERROR();
    glShaderSource(shader, 1, &invalidVert, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glDeleteShader(shader);
}

// Tests bindAttribLocations for reserved prefixes and length limits
TEST_P(WebGLCompatibilityTest, BindAttribLocationLimitation)
{
    constexpr int maxLocStringLength = 256;
    const std::string tooLongString(maxLocStringLength + 1, '_');

    glBindAttribLocation(0, 0, "_webgl_var");

    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glBindAttribLocation(0, 0, static_cast<const GLchar *>(tooLongString.c_str()));

    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test the checks for OOB reads in the vertex buffers, instanced version
TEST_P(WebGL2CompatibilityTest, DrawArraysBufferOutOfBoundsInstanced)
{
    const std::string &vert =
        "attribute float a_pos;\n"
        "attribute float a_w;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_pos, a_pos, a_pos, a_w);\n"
        "}\n";

    const std::string &frag =
        "precision highp float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vert, frag);

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);

    GLint wLocation = glGetAttribLocation(program.get(), "a_w");
    ASSERT_NE(-1, wLocation);

    glUseProgram(program.get());

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);
    glVertexAttribDivisor(posLocation, 1);

    glEnableVertexAttribArray(wLocation);
    glVertexAttribPointer(wLocation, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, 0);
    glVertexAttribDivisor(wLocation, 0);

    const uint8_t* zeroOffset = nullptr;

    // Test touching the last element is valid.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 12);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last element + 1 is invalid.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 13);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test touching the last element is valid, using a stride.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 9);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    ASSERT_GL_NO_ERROR();

    // Test touching the last element + 1 is invalid, using a stride.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 2, zeroOffset + 10);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test any offset is valid if no vertices are drawn.
    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, zeroOffset + 32);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 0);
    ASSERT_GL_NO_ERROR();
}

// Test that having no attributes with a zero divisor is valid in WebGL2
TEST_P(WebGL2CompatibilityTest, InstancedDrawZeroDivisor)
{
    const std::string &vert =
        "attribute float a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_pos, a_pos, a_pos, 1.0);\n"
        "}\n";

    const std::string &frag =
        "precision highp float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vert, frag);

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);

    glUseProgram(program.get());

    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);

    glEnableVertexAttribArray(posLocation);
    glVertexAttribDivisor(posLocation, 1);

    glVertexAttribPointer(0, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, nullptr);
    glDrawArraysInstanced(GL_POINTS, 0, 1, 4);
    ASSERT_GL_NO_ERROR();
}

// Tests that NPOT is not enabled by default in WebGL 1 and that it can be enabled
TEST_P(WebGLCompatibilityTest, NPOT)
{
    EXPECT_FALSE(extensionEnabled("GL_OES_texture_npot"));

    // Create a texture and set an NPOT mip 0, should always be acceptable.
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 10, 10, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    // Try setting an NPOT mip 1 and verify the error if WebGL 1
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 5, 5, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    if (getClientMajorVersion() < 3)
    {
        ASSERT_GL_ERROR(GL_INVALID_VALUE);
    }
    else
    {
        ASSERT_GL_NO_ERROR();
    }

    if (extensionRequestable("GL_OES_texture_npot"))
    {
        glRequestExtensionANGLE("GL_OES_texture_npot");
        ASSERT_GL_NO_ERROR();

        // Try again to set NPOT mip 1
        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 5, 5, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        ASSERT_GL_NO_ERROR();
    }
}

template <typename T>
void FillTexture2D(GLuint texture,
                   GLsizei width,
                   GLsizei height,
                   const T &onePixelData,
                   GLint level,
                   GLint internalFormat,
                   GLenum format,
                   GLenum type)
{
    std::vector<T> allPixelsData(width * height, onePixelData);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, level, internalFormat, width, height, 0, format, type,
                 allPixelsData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// Test that unset gl_Position defaults to (0,0,0,0).
TEST_P(WebGLCompatibilityTest, DefaultPosition)
{
    // Draw a quad where each vertex is red if gl_Position is (0,0,0,0) before it is set,
    // and green otherwise.  The center of each quadrant will be red if and only if all
    // four corners are red.
    const std::string vertexShader =
        "attribute vec3 pos;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "    if (gl_Position == vec4(0,0,0,0)) {\n"
        "        color = vec4(1,0,0,1);\n"
        "    } else {\n"
        "        color = vec4(0,1,0,1);\n"
        "    }\n"
        "    gl_Position = vec4(pos,1);\n"
        "}\n";

    const std::string fragmentShader =
        "precision mediump float;\n"
        "varying vec4 color;\n"
        "void main() {\n"
        "    gl_FragColor = color;\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    drawQuad(program.get(), "pos", 0.0f, 1.0f, true);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 1 / 4, getWindowHeight() * 1 / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 1 / 4, getWindowHeight() * 3 / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 3 / 4, getWindowHeight() * 1 / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 3 / 4, getWindowHeight() * 3 / 4, GLColor::red);
}

// Tests that a rendering feedback loop triggers a GL error under WebGL.
// Based on WebGL test conformance/renderbuffers/feedback-loop.html.
TEST_P(WebGLCompatibilityTest, RenderingFeedbackLoop)
{
    const std::string vertexShader =
        "attribute vec4 a_position;\n"
        "varying vec2 v_texCoord;\n"
        "void main() {\n"
        "    gl_Position = a_position;\n"
        "    v_texCoord = (a_position.xy * 0.5) + 0.5;\n"
        "}\n";

    const std::string fragmentShader =
        "precision mediump float;\n"
        "varying vec2 v_texCoord;\n"
        "uniform sampler2D u_texture;\n"
        "void main() {\n"
        "    // Shader swizzles color channels so we can tell if the draw succeeded.\n"
        "    gl_FragColor = texture2D(u_texture, v_texCoord).gbra;\n"
        "}\n";

    GLTexture texture;
    FillTexture2D(texture.get(), 1, 1, GLColor::red, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);

    ASSERT_GL_NO_ERROR();

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);

    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    GLint uniformLoc = glGetUniformLocation(program.get(), "u_texture");
    ASSERT_NE(-1, uniformLoc);

    glUseProgram(program.get());
    glUniform1i(uniformLoc, 0);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    ASSERT_GL_NO_ERROR();

    // Drawing with a texture that is also bound to the current framebuffer should fail
    glBindTexture(GL_TEXTURE_2D, texture.get());
    drawQuad(program.get(), "a_position", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Ensure that the texture contents did not change after the previous render
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    drawQuad(program.get(), "a_position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Drawing when texture is bound to an inactive uniform should succeed
    GLTexture texture2;
    FillTexture2D(texture2.get(), 1, 1, GLColor::green, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture.get());
    drawQuad(program.get(), "a_position", 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Test for the max draw buffers and color attachments.
TEST_P(WebGLCompatibilityTest, MaxDrawBuffersAttachmentPoints)
{
    // This test only applies to ES2.
    if (getClientMajorVersion() != 2)
    {
        return;
    }

    GLFramebuffer fbo[2];
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0].get());

    // Test that is valid when we bind with a single attachment point.
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);
    ASSERT_GL_NO_ERROR();

    // Test that enabling the draw buffers extension will allow us to bind with a non-zero
    // attachment point.
    if (extensionRequestable("GL_EXT_draw_buffers"))
    {
        glRequestExtensionANGLE("GL_EXT_draw_buffers");
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(extensionEnabled("GL_EXT_draw_buffers"));

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[1].get());

        GLTexture texture2;
        glBindTexture(GL_TEXTURE_2D, texture2.get());
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texture2.get(),
                               0);
        ASSERT_GL_NO_ERROR();
    }
}

// Test that the offset in the index buffer is forced to be a multiple of the element size
TEST_P(WebGLCompatibilityTest, DrawElementsOffsetRestriction)
{
    const std::string &vert =
        "attribute vec3 a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(a_pos, 1.0);\n"
        "}\n";

    const std::string &frag =
        "precision highp float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vert, frag);

    GLint posLocation = glGetAttribLocation(program.get(), "a_pos");
    ASSERT_NE(-1, posLocation);
    glUseProgram(program.get());

    const auto &vertices = GetQuadVertices();

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer.get());
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(posLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posLocation);

    GLBuffer indexBuffer;
    const GLubyte indices[] = {0, 0, 0, 0, 0, 0, 0, 0};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer.get());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    ASSERT_GL_NO_ERROR();

    const char *zeroIndices = nullptr;

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, zeroIndices);
    ASSERT_GL_NO_ERROR();

    glDrawElements(GL_TRIANGLES, 4, GL_UNSIGNED_SHORT, zeroIndices);
    ASSERT_GL_NO_ERROR();

    glDrawElements(GL_TRIANGLES, 4, GL_UNSIGNED_SHORT, zeroIndices + 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that the offset and stride in the vertex buffer is forced to be a multiple of the element
// size
TEST_P(WebGLCompatibilityTest, VertexAttribPointerOffsetRestriction)
{
    const char *zeroOffset = nullptr;

    // Base case, vector of two floats
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, zeroOffset);
    ASSERT_GL_NO_ERROR();

    // Test setting a non-multiple offset
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, zeroOffset + 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, zeroOffset + 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, zeroOffset + 3);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Test setting a non-multiple stride
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 1, zeroOffset);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2, zeroOffset);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3, zeroOffset);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

void WebGLCompatibilityTest::drawBuffersEXTFeedbackLoop(GLuint program,
                                                        const std::array<GLenum, 2> &drawBuffers,
                                                        GLenum expectedError)
{
    glDrawBuffersEXT(2, drawBuffers.data());

    // Make sure framebuffer is complete before feedback loop detection
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    drawQuad(program, "aPosition", 0.5f, 1.0f, true);

    // "Rendering to a texture where it samples from should geneates INVALID_OPERATION. Otherwise,
    // it should be NO_ERROR"
    EXPECT_GL_ERROR(expectedError);
}

// This tests that rendering feedback loops works as expected with GL_EXT_draw_buffers.
// Based on WebGL test conformance/extensions/webgl-draw-buffers-feedback-loop.html
TEST_P(WebGLCompatibilityTest, RenderingFeedbackLoopWithDrawBuffersEXT)
{
    const std::string vertexShader =
        "attribute vec4 aPosition;\n"
        "varying vec2 texCoord;\n"
        "void main() {\n"
        "    gl_Position = aPosition;\n"
        "    texCoord = (aPosition.xy * 0.5) + 0.5;\n"
        "}\n";

    const std::string fragmentShader =
        "#extension GL_EXT_draw_buffers : require\n"
        "precision mediump float;\n"
        "uniform sampler2D tex;\n"
        "varying vec2 texCoord;\n"
        "void main() {\n"
        "    gl_FragData[0] = texture2D(tex, texCoord);\n"
        "    gl_FragData[1] = texture2D(tex, texCoord);\n"
        "}\n";

    GLsizei width  = 8;
    GLsizei height = 8;

    // This shader cannot be run in ES3, because WebGL 2 does not expose the draw buffers
    // extension and gl_FragData semantics are changed to enforce indexing by zero always.
    // TODO(jmadill): This extension should be disabled in WebGL 2 contexts.
    if (/*!extensionEnabled("GL_EXT_draw_buffers")*/ getClientMajorVersion() != 2)
    {
        // No WEBGL_draw_buffers support -- this is legal.
        return;
    }

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);

    if (maxDrawBuffers < 2)
    {
        std::cout << "Test skipped because MAX_DRAW_BUFFERS is too small." << std::endl;
        return;
    }

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    glUseProgram(program.get());
    glViewport(0, 0, width, height);

    GLTexture tex0;
    GLTexture tex1;
    GLFramebuffer fbo;
    FillTexture2D(tex0.get(), width, height, GLColor::red, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    FillTexture2D(tex1.get(), width, height, GLColor::green, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, tex1.get());
    GLint texLoc = glGetUniformLocation(program.get(), "tex");
    ASSERT_NE(-1, texLoc);
    glUniform1i(texLoc, 0);
    ASSERT_GL_NO_ERROR();

    // The sampling texture is bound to COLOR_ATTACHMENT1 during resource allocation
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0.get(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex1.get(), 0);

    drawBuffersEXTFeedbackLoop(program.get(), {{GL_NONE, GL_COLOR_ATTACHMENT1}},
                               GL_INVALID_OPERATION);
    drawBuffersEXTFeedbackLoop(program.get(), {{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}},
                               GL_INVALID_OPERATION);
    drawBuffersEXTFeedbackLoop(program.get(), {{GL_COLOR_ATTACHMENT0, GL_NONE}}, GL_NO_ERROR);
}

// Test tests that texture copying feedback loops are properly rejected in WebGL.
// Based on the WebGL test conformance/textures/misc/texture-copying-feedback-loops.html
TEST_P(WebGLCompatibilityTest, TextureCopyingFeedbackLoops)
{
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLTexture texture2;
    glBindTexture(GL_TEXTURE_2D, texture2.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);

    // framebuffer should be FRAMEBUFFER_COMPLETE.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    ASSERT_GL_NO_ERROR();

    // testing copyTexImage2D

    // copyTexImage2D to same texture but different level
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glCopyTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 0, 0, 2, 2, 0);
    EXPECT_GL_NO_ERROR();

    // copyTexImage2D to same texture same level, invalid feedback loop
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 2, 2, 0);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // copyTexImage2D to different texture
    glBindTexture(GL_TEXTURE_2D, texture2.get());
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 2, 2, 0);
    EXPECT_GL_NO_ERROR();

    // testing copyTexSubImage2D

    // copyTexSubImage2D to same texture but different level
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glCopyTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 0, 0, 1, 1);
    EXPECT_GL_NO_ERROR();

    // copyTexSubImage2D to same texture same level, invalid feedback loop
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // copyTexSubImage2D to different texture
    glBindTexture(GL_TEXTURE_2D, texture2.get());
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    EXPECT_GL_NO_ERROR();
}

void WebGLCompatibilityTest::drawBuffersFeedbackLoop(GLuint program,
                                                     const std::array<GLenum, 2> &drawBuffers,
                                                     GLenum expectedError)
{
    glDrawBuffers(2, drawBuffers.data());

    // Make sure framebuffer is complete before feedback loop detection
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    drawQuad(program, "aPosition", 0.5f, 1.0f, true);

    // "Rendering to a texture where it samples from should geneates INVALID_OPERATION. Otherwise,
    // it should be NO_ERROR"
    EXPECT_GL_ERROR(expectedError);
}

// Tests invariance matching rules between built in varyings.
// Based on WebGL test conformance/glsl/misc/shaders-with-invariance.html.
TEST_P(WebGLCompatibilityTest, BuiltInInvariant)
{
    const std::string vertexShaderVariant =
        "varying vec4 v_varying;\n"
        "void main()\n"
        "{\n"
        "    gl_PointSize = 1.0;\n"
        "    gl_Position = v_varying;\n"
        "}";
    const std::string fragmentShaderInvariantGlFragCoord =
        "invariant gl_FragCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = gl_FragCoord;\n"
        "}";
    const std::string fragmentShaderInvariantGlPointCoord =
        "invariant gl_PointCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(gl_PointCoord, 0.0, 0.0);\n"
        "}";

    GLuint program = CompileProgram(vertexShaderVariant, fragmentShaderInvariantGlFragCoord);
    EXPECT_EQ(0u, program);

    program = CompileProgram(vertexShaderVariant, fragmentShaderInvariantGlPointCoord);
    EXPECT_EQ(0u, program);
}

// Tests global namespace conflicts between uniforms and attributes.
// Based on WebGL test conformance/glsl/misc/shaders-with-name-conflicts.html.
TEST_P(WebGLCompatibilityTest, GlobalNamesConflict)
{
    const std::string vertexShader =
        "attribute vec4 foo;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = foo;\n"
        "}";
    const std::string fragmentShader =
        "precision mediump float;\n"
        "uniform vec4 foo;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = foo;\n"
        "}";

    GLuint program = CompileProgram(vertexShader, fragmentShader);
    EXPECT_EQ(0u, program);
}

// Test dimension and image size validation of compressed textures
TEST_P(WebGLCompatibilityTest, CompressedTextureS3TC)
{
    if (extensionRequestable("GL_EXT_texture_compression_dxt1"))
    {
        glRequestExtensionANGLE("GL_EXT_texture_compression_dxt1");
    }

    if (!extensionEnabled("GL_EXT_texture_compression_dxt1"))
    {
        std::cout << "Test skipped because GL_EXT_texture_compression_dxt1 is not available."
                  << std::endl;
        return;
    }

    constexpr uint8_t CompressedImageDXT1[] = {0x00, 0xf8, 0x00, 0xf8, 0xaa, 0xaa, 0xaa, 0xaa};

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    // Regular case, verify that it works
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    // Test various dimensions that are not valid
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 3, 4, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 3, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 2, 2, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 1, 1, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Test various image sizes that are not valid
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1) - 1, CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1) + 1, CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 0,
                           CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 0, 0, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);

    // Fill a full mip chain and verify that it works
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 2, 2, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    glCompressedTexImage2D(GL_TEXTURE_2D, 2, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 1, 1, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    // Test that non-block size sub-uploads are not valid for the 0 mip
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 2, 2, 2, 2, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Test that non-block size sub-uploads are valid for if they fill the whole mip
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 2, 0, 0, 1, 1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    // Test that if the format miss-matches the texture, an error is generated
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 2, 2, 2, 2, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                              sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

TEST_P(WebGLCompatibilityTest, L32FTextures)
{
    constexpr float textureData[]   = {15.1f, 0.0f, 0.0f, 0.0f};
    constexpr float readPixelData[] = {textureData[0], textureData[0], textureData[0], 1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized L 32F
        {
            bool texture = extensionEnabled("GL_OES_texture_float");
            bool filter  = extensionEnabled("GL_OES_texture_float_linear");
            bool render  = false;
            TestFloatTextureFormat(GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT, texture, filter, render,
                                   textureData, readPixelData);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized L 32F
            bool texture = extensionEnabled("GL_OES_texture_float") &&
                           extensionEnabled("GL_EXT_texture_storage");
            bool filter = extensionEnabled("GL_OES_texture_float_linear");
            bool render = false;
            TestFloatTextureFormat(GL_LUMINANCE32F_EXT, GL_LUMINANCE, GL_FLOAT, texture, filter,
                                   render, textureData, readPixelData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, A32FTextures)
{
    constexpr float textureData[]   = {33.33f, 0.0f, 0.0f, 0.0f};
    constexpr float readPixelData[] = {0.0f, 0.0f, 0.0f, textureData[0]};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized A 32F
        {
            bool texture = extensionEnabled("GL_OES_texture_float");
            bool filter  = extensionEnabled("GL_OES_texture_float_linear");
            bool render  = false;
            TestFloatTextureFormat(GL_ALPHA, GL_ALPHA, GL_FLOAT, texture, filter, render,
                                   textureData, readPixelData);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized A 32F
            bool texture = extensionEnabled("GL_OES_texture_float") &&
                           extensionEnabled("GL_EXT_texture_storage");
            bool filter = extensionEnabled("GL_OES_texture_float_linear");
            bool render = false;
            TestFloatTextureFormat(GL_ALPHA32F_EXT, GL_ALPHA, GL_FLOAT, texture, filter, render,
                                   textureData, readPixelData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, LA32FTextures)
{
    constexpr float textureData[]   = {-0.21f, 15.1f, 0.0f, 0.0f};
    constexpr float readPixelData[] = {textureData[0], textureData[0], textureData[0],
                                       textureData[1]};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized LA 32F
        {
            bool texture = extensionEnabled("GL_OES_texture_float");
            bool filter  = extensionEnabled("GL_OES_texture_float_linear");
            bool render  = false;
            TestFloatTextureFormat(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT, texture,
                                   filter, render, textureData, readPixelData);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized LA 32F
            bool texture = extensionEnabled("GL_OES_texture_float") &&
                           extensionEnabled("GL_EXT_texture_storage");
            bool filter = extensionEnabled("GL_OES_texture_float_linear");
            bool render = false;
            TestFloatTextureFormat(GL_LUMINANCE_ALPHA32F_EXT, GL_LUMINANCE_ALPHA, GL_FLOAT, texture,
                                   filter, render, textureData, readPixelData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, R32FTextures)
{
    constexpr float data[] = {1000.0f, 0.0f, 0.0f, 1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized R 32F
        {
            bool texture =
                extensionEnabled("GL_OES_texture_float") && extensionEnabled("GL_EXT_texture_rg");
            bool filter = extensionEnabled("GL_OES_texture_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RED, GL_RED, GL_FLOAT, texture, filter, render, data, data);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized R 32F
            bool texture =
                (getClientMajorVersion() >= 3) || (extensionEnabled("GL_OES_texture_float") &&
                                                   extensionEnabled("GL_EXT_texture_rg") &&
                                                   extensionEnabled("GL_EXT_texture_storage"));
            bool filter = extensionEnabled("GL_OES_texture_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_R32F, GL_RED, GL_FLOAT, texture, filter, render, data, data);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RG32FTextures)
{
    constexpr float data[] = {1000.0f, -0.001f, 0.0f, 1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RG 32F
        {
            bool texture =
                (extensionEnabled("GL_OES_texture_float") && extensionEnabled("GL_EXT_texture_rg"));
            bool filter = extensionEnabled("GL_OES_texture_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RG, GL_RG, GL_FLOAT, texture, filter, render, data, data);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RG 32F
            bool texture =
                (getClientMajorVersion() >= 3) || (extensionEnabled("GL_OES_texture_float") &&
                                                   extensionEnabled("GL_EXT_texture_rg") &&
                                                   extensionEnabled("GL_EXT_texture_storage"));
            bool filter = extensionEnabled("GL_OES_texture_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RG32F, GL_RG, GL_FLOAT, texture, filter, render, data, data);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RGB32FTextures)
{
    if (IsLinux() && IsIntel())
    {
        std::cout << "Test skipped on Linux Intel." << std::endl;
        return;
    }

    constexpr float data[] = {1000.0f, -500.0f, 10.0f, 1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RGB 32F
        {
            bool texture = extensionEnabled("GL_OES_texture_float");
            bool filter  = extensionEnabled("GL_OES_texture_float_linear");
            bool render  = extensionEnabled("GL_CHROMIUM_color_buffer_float_rgb");
            TestFloatTextureFormat(GL_RGB, GL_RGB, GL_FLOAT, texture, filter, render, data, data);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RGBA 32F
            bool texture =
                (getClientMajorVersion() >= 3) || (extensionEnabled("GL_OES_texture_float") &&
                                                   extensionEnabled("GL_EXT_texture_storage"));
            bool filter = extensionEnabled("GL_OES_texture_float_linear");
            bool render = extensionEnabled("GL_CHROMIUM_color_buffer_float_rgb");
            TestFloatTextureFormat(GL_RGB32F, GL_RGB, GL_FLOAT, texture, filter, render, data,
                                   data);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RGBA32FTextures)
{
    constexpr float data[] = {7000.0f, 100.0f, 33.0f, -1.0f};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RGBA 32F
        {
            bool texture = extensionEnabled("GL_OES_texture_float");
            bool filter  = extensionEnabled("GL_OES_texture_float_linear");
            bool render  = extensionEnabled("GL_EXT_color_buffer_float") ||
                          extensionEnabled("GL_CHROMIUM_color_buffer_float_rgba");
            TestFloatTextureFormat(GL_RGBA, GL_RGBA, GL_FLOAT, texture, filter, render, data, data);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RGBA 32F
            bool texture =
                (getClientMajorVersion() >= 3) || (extensionEnabled("GL_OES_texture_float") &&
                                                   extensionEnabled("GL_EXT_texture_storage"));
            bool filter = extensionEnabled("GL_OES_texture_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_float") ||
                          extensionEnabled("GL_CHROMIUM_color_buffer_float_rgba");
            TestFloatTextureFormat(GL_RGBA32F, GL_RGBA, GL_FLOAT, texture, filter, render, data,
                                   data);
        }
    }
}

TEST_P(WebGLCompatibilityTest, R16FTextures)
{
    constexpr float readPixelsData[] = {-5000.0f, 0.0f, 0.0f, 1.0f};
    const GLushort textureData[]     = {
        gl::float32ToFloat16(readPixelsData[0]), gl::float32ToFloat16(readPixelsData[1]),
        gl::float32ToFloat16(readPixelsData[2]), gl::float32ToFloat16(readPixelsData[3])};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized R 16F (OES)
        {
            bool texture = extensionEnabled("GL_OES_texture_half_float") &&
                           extensionEnabled("GL_EXT_texture_rg");
            bool filter = getClientMajorVersion() >= 3 ||
                          extensionEnabled("GL_OES_texture_half_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_RED, GL_RED, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }

        // Unsized R 16F
        {
            bool texture = false;
            bool filter  = false;
            bool render  = false;
            TestFloatTextureFormat(GL_RED, GL_RED, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized R 16F
            bool texture = getClientMajorVersion() >= 3;
            bool filter  = getClientMajorVersion() >= 3 ||
                          extensionEnabled("GL_OES_texture_half_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_half_float") ||
                          extensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_R16F, GL_RED, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RG16FTextures)
{
    constexpr float readPixelsData[] = {7108.0f, -10.0f, 0.0f, 1.0f};
    const GLushort textureData[]     = {
        gl::float32ToFloat16(readPixelsData[0]), gl::float32ToFloat16(readPixelsData[1]),
        gl::float32ToFloat16(readPixelsData[2]), gl::float32ToFloat16(readPixelsData[3])};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RG 16F (OES)
        {
            bool texture = extensionEnabled("GL_OES_texture_half_float") &&
                           extensionEnabled("GL_EXT_texture_rg");
            bool filter = getClientMajorVersion() >= 3 ||
                          extensionEnabled("GL_OES_texture_half_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_half_float") &&
                          extensionEnabled("GL_EXT_texture_rg");
            TestFloatTextureFormat(GL_RG, GL_RG, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }

        // Unsized RG 16F
        {
            bool texture = false;
            bool filter  = false;
            bool render  = false;
            TestFloatTextureFormat(GL_RG, GL_RG, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RG 16F
            bool texture = getClientMajorVersion() >= 3;
            bool filter  = getClientMajorVersion() >= 3 ||
                          extensionEnabled("GL_OES_texture_half_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_half_float") ||
                          extensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RG16F, GL_RG, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RGB16FTextures)
{
    if (IsOzone() && IsIntel())
    {
        std::cout << "Test skipped on Intel Ozone." << std::endl;
        return;
    }

    constexpr float readPixelsData[] = {7000.0f, 100.0f, 33.0f, 1.0f};
    const GLushort textureData[]     = {
        gl::float32ToFloat16(readPixelsData[0]), gl::float32ToFloat16(readPixelsData[1]),
        gl::float32ToFloat16(readPixelsData[2]), gl::float32ToFloat16(readPixelsData[3])};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RGB 16F (OES)
        {
            bool texture = extensionEnabled("GL_OES_texture_half_float");
            bool filter  = getClientMajorVersion() >= 3 ||
                          extensionEnabled("GL_OES_texture_half_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_RGB, GL_RGB, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }

        // Unsized RGB 16F
        {
            bool texture = false;
            bool filter  = false;
            bool render  = false;
            TestFloatTextureFormat(GL_RGB, GL_RGB, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RGB 16F
            bool texture = getClientMajorVersion() >= 3;
            bool filter  = getClientMajorVersion() >= 3 ||
                          extensionEnabled("GL_OES_texture_half_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_half_float");
            TestFloatTextureFormat(GL_RGB16F, GL_RGB, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }
    }
}

TEST_P(WebGLCompatibilityTest, RGBA16FTextures)
{
    if (IsOzone() && IsIntel())
    {
        std::cout << "Test skipped on Intel Ozone." << std::endl;
        return;
    }

    constexpr float readPixelsData[] = {7000.0f, 100.0f, 33.0f, -1.0f};
    const GLushort textureData[]     = {
        gl::float32ToFloat16(readPixelsData[0]), gl::float32ToFloat16(readPixelsData[1]),
        gl::float32ToFloat16(readPixelsData[2]), gl::float32ToFloat16(readPixelsData[3])};

    for (auto extension : FloatingPointTextureExtensions)
    {
        if (strlen(extension) > 0 && extensionRequestable(extension))
        {
            glRequestExtensionANGLE(extension);
            ASSERT_GL_NO_ERROR();
        }

        // Unsized RGBA 16F (OES)
        {
            bool texture = extensionEnabled("GL_OES_texture_half_float");
            bool filter  = getClientMajorVersion() >= 3 ||
                          extensionEnabled("GL_OES_texture_half_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_half_float") ||
                          extensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES, texture, filter, render,
                                   textureData, readPixelsData);
        }

        // Unsized RGBA 16F
        {
            bool texture = false;
            bool filter  = false;
            bool render  = false;
            TestFloatTextureFormat(GL_RGBA, GL_RGBA, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }

        if (getClientMajorVersion() >= 3 || extensionEnabled("GL_EXT_texture_storage"))
        {
            // Sized RGBA 16F
            bool texture = getClientMajorVersion() >= 3;
            bool filter  = getClientMajorVersion() >= 3 ||
                          extensionEnabled("GL_OES_texture_half_float_linear");
            bool render = extensionEnabled("GL_EXT_color_buffer_half_float") ||
                          extensionEnabled("GL_EXT_color_buffer_float");
            TestFloatTextureFormat(GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, texture, filter, render,
                                   textureData, readPixelsData);
        }
    }
}

// Test that when GL_CHROMIUM_color_buffer_float_rgb[a] is enabled, sized GL_RGB[A]_32F formats are
// accepted by glTexImage2D
TEST_P(WebGLCompatibilityTest, SizedRGBA32FFormats)
{
    if (getClientMajorVersion() != 2)
    {
        std::cout << "Test skipped because it is only valid for WebGL1 contexts." << std::endl;
        return;
    }

    if (!extensionRequestable("GL_OES_texture_float"))
    {
        std::cout << "Test skipped because GL_OES_texture_float is not requestable." << std::endl;
        return;
    }
    glRequestExtensionANGLE("GL_OES_texture_float");
    ASSERT_GL_NO_ERROR();

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 1, 1, 0, GL_RGB, GL_FLOAT, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (extensionRequestable("GL_CHROMIUM_color_buffer_float_rgba"))
    {
        glRequestExtensionANGLE("GL_CHROMIUM_color_buffer_float_rgba");
        ASSERT_GL_NO_ERROR();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
        EXPECT_GL_NO_ERROR();
    }

    if (extensionRequestable("GL_CHROMIUM_color_buffer_float_rgb"))
    {
        glRequestExtensionANGLE("GL_CHROMIUM_color_buffer_float_rgb");
        ASSERT_GL_NO_ERROR();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 1, 1, 0, GL_RGB, GL_FLOAT, nullptr);
        EXPECT_GL_NO_ERROR();
    }
}

// This tests that rendering feedback loops works as expected with WebGL 2.
// Based on WebGL test conformance2/rendering/rendering-sampling-feedback-loop.html
TEST_P(WebGL2CompatibilityTest, RenderingFeedbackLoopWithDrawBuffers)
{
    const std::string vertexShader =
        "#version 300 es\n"
        "in vec4 aPosition;\n"
        "out vec2 texCoord;\n"
        "void main() {\n"
        "    gl_Position = aPosition;\n"
        "    texCoord = (aPosition.xy * 0.5) + 0.5;\n"
        "}\n";

    const std::string fragmentShader =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform sampler2D tex;\n"
        "in vec2 texCoord;\n"
        "out vec4 oColor;\n"
        "void main() {\n"
        "    oColor = texture(tex, texCoord);\n"
        "}\n";

    GLsizei width  = 8;
    GLsizei height = 8;

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    // ES3 requires a minimum value of 4 for MAX_DRAW_BUFFERS.
    ASSERT_GE(maxDrawBuffers, 2);

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    glUseProgram(program.get());
    glViewport(0, 0, width, height);

    GLTexture tex0;
    GLTexture tex1;
    GLFramebuffer fbo;
    FillTexture2D(tex0.get(), width, height, GLColor::red, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    FillTexture2D(tex1.get(), width, height, GLColor::green, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, tex1.get());
    GLint texLoc = glGetUniformLocation(program.get(), "tex");
    ASSERT_NE(-1, texLoc);
    glUniform1i(texLoc, 0);

    // The sampling texture is bound to COLOR_ATTACHMENT1 during resource allocation
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0.get(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex1.get(), 0);
    ASSERT_GL_NO_ERROR();

    drawBuffersFeedbackLoop(program.get(), {{GL_NONE, GL_COLOR_ATTACHMENT1}}, GL_INVALID_OPERATION);
    drawBuffersFeedbackLoop(program.get(), {{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}},
                            GL_INVALID_OPERATION);
    drawBuffersFeedbackLoop(program.get(), {{GL_COLOR_ATTACHMENT0, GL_NONE}}, GL_NO_ERROR);
}

// This test covers detection of rendering feedback loops between the FBO and a depth Texture.
// Based on WebGL test conformance2/rendering/depth-stencil-feedback-loop.html
TEST_P(WebGL2CompatibilityTest, RenderingFeedbackLoopWithDepthStencil)
{
    const std::string vertexShader =
        "#version 300 es\n"
        "in vec4 aPosition;\n"
        "out vec2 texCoord;\n"
        "void main() {\n"
        "    gl_Position = aPosition;\n"
        "    texCoord = (aPosition.xy * 0.5) + 0.5;\n"
        "}\n";

    const std::string fragmentShader =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform sampler2D tex;\n"
        "in vec2 texCoord;\n"
        "out vec4 oColor;\n"
        "void main() {\n"
        "    oColor = texture(tex, texCoord);\n"
        "}\n";

    GLsizei width  = 8;
    GLsizei height = 8;

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    glUseProgram(program.get());

    glViewport(0, 0, width, height);

    GLint texLoc = glGetUniformLocation(program.get(), "tex");
    glUniform1i(texLoc, 0);

    // Create textures and allocate storage
    GLTexture tex0;
    GLTexture tex1;
    GLRenderbuffer rb;
    FillTexture2D(tex0.get(), width, height, GLColor::black, 0, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    FillTexture2D(tex1.get(), width, height, 0x80, 0, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT,
                  GL_UNSIGNED_INT);
    glBindRenderbuffer(GL_RENDERBUFFER, rb.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
    ASSERT_GL_NO_ERROR();

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0.get(), 0);

    // Test rendering and sampling feedback loop for depth buffer
    glBindTexture(GL_TEXTURE_2D, tex1.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex1.get(), 0);
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // The same image is used as depth buffer during rendering.
    glEnable(GL_DEPTH_TEST);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // The same image is used as depth buffer. But depth mask is false.
    glDepthMask(GL_FALSE);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_NO_ERROR();

    // The same image is used as depth buffer. But depth test is not enabled during rendering.
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_NO_ERROR();

    // Test rendering and sampling feedback loop for stencil buffer
    glBindTexture(GL_RENDERBUFFER, rb.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb.get());
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    constexpr GLint stencilClearValue = 0x40;
    glClearBufferiv(GL_STENCIL, 0, &stencilClearValue);

    // The same image is used as stencil buffer during rendering.
    glEnable(GL_STENCIL_TEST);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // The same image is used as stencil buffer. But stencil mask is zero.
    glStencilMask(0x0);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_NO_ERROR();

    // The same image is used as stencil buffer. But stencil test is not enabled during rendering.
    glStencilMask(0xffff);
    glDisable(GL_STENCIL_TEST);
    drawQuad(program.get(), "aPosition", 0.5f, 1.0f, true);
    EXPECT_GL_NO_ERROR();
}

// The source and the target for CopyTexSubImage3D are the same 3D texture.
// But the level of the 3D texture != the level of the read attachment.
TEST_P(WebGL2CompatibilityTest, NoTextureCopyingFeedbackLoopBetween3DLevels)
{
    GLTexture texture;
    GLFramebuffer framebuffer;

    glBindTexture(GL_TEXTURE_3D, texture.get());
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture.get(), 0, 0);
    ASSERT_GL_NO_ERROR();

    glCopyTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 0, 0, 0, 2, 2);
    EXPECT_GL_NO_ERROR();
}

// The source and the target for CopyTexSubImage3D are the same 3D texture.
// But the zoffset of the 3D texture != the layer of the read attachment.
TEST_P(WebGL2CompatibilityTest, NoTextureCopyingFeedbackLoopBetween3DLayers)
{
    GLTexture texture;
    GLFramebuffer framebuffer;

    glBindTexture(GL_TEXTURE_3D, texture.get());
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture.get(), 0, 1);
    ASSERT_GL_NO_ERROR();

    glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 2, 2);
    EXPECT_GL_NO_ERROR();
}

// The source and the target for CopyTexSubImage3D are the same 3D texture.
// And the level / zoffset of the 3D texture is equal to the level / layer of the read attachment.
TEST_P(WebGL2CompatibilityTest, TextureCopyingFeedbackLoop3D)
{
    GLTexture texture;
    GLFramebuffer framebuffer;

    glBindTexture(GL_TEXTURE_3D, texture.get());
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage3D(GL_TEXTURE_3D, 2, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture.get(), 1, 0);
    ASSERT_GL_NO_ERROR();

    glCopyTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 0, 0, 0, 2, 2);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that errors are generated when there isn't a defined conversion between the clear type and
// the buffer type.
TEST_P(WebGL2CompatibilityTest, ClearBufferTypeCompatibity)
{
    if (IsD3D11())
    {
        std::cout << "Test skipped because it generates D3D11 runtime warnings." << std::endl;
        return;
    }

    constexpr float clearFloat[]       = {0.0f, 0.0f, 0.0f, 0.0f};
    constexpr int clearInt[]           = {0, 0, 0, 0};
    constexpr unsigned int clearUint[] = {0, 0, 0, 0};

    GLTexture texture;
    GLFramebuffer framebuffer;

    glBindTexture(GL_TEXTURE_2D, texture.get());
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);
    ASSERT_GL_NO_ERROR();

    // Unsigned integer buffer
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 1, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);
    ASSERT_GL_NO_ERROR();

    glClearBufferfv(GL_COLOR, 0, clearFloat);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferiv(GL_COLOR, 0, clearInt);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferuiv(GL_COLOR, 0, clearUint);
    EXPECT_GL_NO_ERROR();

    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Integer buffer
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 1, 1, 0, GL_RGBA_INTEGER, GL_INT, nullptr);
    ASSERT_GL_NO_ERROR();

    glClearBufferfv(GL_COLOR, 0, clearFloat);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferiv(GL_COLOR, 0, clearInt);
    EXPECT_GL_NO_ERROR();

    glClearBufferuiv(GL_COLOR, 0, clearUint);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Float buffer
    if (extensionRequestable("GL_EXT_color_buffer_float"))
    {
        glRequestExtensionANGLE("GL_EXT_color_buffer_float");
    }

    if (extensionEnabled("GL_EXT_color_buffer_float"))
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
        ASSERT_GL_NO_ERROR();

        glClearBufferfv(GL_COLOR, 0, clearFloat);
        EXPECT_GL_NO_ERROR();

        glClearBufferiv(GL_COLOR, 0, clearInt);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glClearBufferuiv(GL_COLOR, 0, clearUint);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_GL_NO_ERROR();
    }

    // Normalized uint buffer
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_GL_NO_ERROR();

    glClearBufferfv(GL_COLOR, 0, clearFloat);
    EXPECT_GL_NO_ERROR();

    glClearBufferiv(GL_COLOR, 0, clearInt);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferuiv(GL_COLOR, 0, clearUint);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_NO_ERROR();
}

// Test the interaction of WebGL compatibility clears with default framebuffers
TEST_P(WebGL2CompatibilityTest, ClearBufferDefaultFramebuffer)
{
    constexpr float clearFloat[]       = {0.0f, 0.0f, 0.0f, 0.0f};
    constexpr int clearInt[]           = {0, 0, 0, 0};
    constexpr unsigned int clearUint[] = {0, 0, 0, 0};

    // glClear works as usual, this is also a regression test for a bug where we
    // iterated on maxDrawBuffers for default framebuffers, triggering an assert
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_NO_ERROR();

    // Default framebuffers are normalized uints, so only glClearBufferfv works.
    glClearBufferfv(GL_COLOR, 0, clearFloat);
    EXPECT_GL_NO_ERROR();

    glClearBufferiv(GL_COLOR, 0, clearInt);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glClearBufferuiv(GL_COLOR, 0, clearUint);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that errors are generate when trying to blit from an image to itself
TEST_P(WebGL2CompatibilityTest, BlitFramebufferSameImage)
{
    GLTexture textures[2];
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);

    GLRenderbuffer renderbuffers[2];
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 4, 4);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 4, 4);

    GLFramebuffer framebuffers[2];
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[1]);

    ASSERT_GL_NO_ERROR();

    // Same texture
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0],
                           0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0],
                           0);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Same textures but different renderbuffers
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffers[0]);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffers[1]);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                      GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                      GL_NEAREST);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Same renderbuffers but different textures
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0],
                           0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1],
                           0);
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffers[0]);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuffers[0]);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                      GL_NEAREST);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                      GL_NEAREST);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that errors are generated when the fragment shader output doesn't match the bound color
// buffer types
TEST_P(WebGL2CompatibilityTest, FragmentShaderColorBufferTypeMissmatch)
{
    const std::string vertexShader =
        "#version 300 es\n"
        "void main() {\n"
        "    gl_Position = vec4(0, 0, 0, 1);\n"
        "}\n";

    const std::string fragmentShader =
        "#version 300 es\n"
        "precision mediump float;\n"
        "layout(location = 0) out vec4 floatOutput;\n"
        "layout(location = 1) out uvec4 uintOutput;\n"
        "layout(location = 2) out ivec4 intOutput;\n"
        "void main() {\n"
        "    floatOutput = vec4(0, 0, 0, 1);\n"
        "    uintOutput = uvec4(0, 0, 0, 1);\n"
        "    intOutput = ivec4(0, 0, 0, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    glUseProgram(program.get());

    GLuint floatLocation = glGetFragDataLocation(program, "floatOutput");
    GLuint uintLocation  = glGetFragDataLocation(program, "uintOutput");
    GLuint intLocation   = glGetFragDataLocation(program, "intOutput");

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLRenderbuffer floatRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, floatRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + floatLocation, GL_RENDERBUFFER,
                              floatRenderbuffer);

    GLRenderbuffer uintRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, uintRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8UI, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + uintLocation, GL_RENDERBUFFER,
                              uintRenderbuffer);

    GLRenderbuffer intRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, intRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8I, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + intLocation, GL_RENDERBUFFER,
                              intRenderbuffer);

    ASSERT_GL_NO_ERROR();

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    std::vector<GLenum> drawBuffers(static_cast<size_t>(maxDrawBuffers), GL_NONE);
    drawBuffers[floatLocation] = GL_COLOR_ATTACHMENT0 + floatLocation;
    drawBuffers[uintLocation]  = GL_COLOR_ATTACHMENT0 + uintLocation;
    drawBuffers[intLocation]   = GL_COLOR_ATTACHMENT0 + intLocation;

    glDrawBuffers(maxDrawBuffers, drawBuffers.data());

    // Check that the correct case generates no errors
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Unbind some buffers and verify that there are still no errors
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + uintLocation, GL_RENDERBUFFER,
                              0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + intLocation, GL_RENDERBUFFER,
                              0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Swap the int and uint buffers to and verify that an error is generated
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + uintLocation, GL_RENDERBUFFER,
                              intRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + intLocation, GL_RENDERBUFFER,
                              uintRenderbuffer);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Swap the float and uint buffers to and verify that an error is generated
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + uintLocation, GL_RENDERBUFFER,
                              floatRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + floatLocation, GL_RENDERBUFFER,
                              uintRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + intLocation, GL_RENDERBUFFER,
                              intRenderbuffer);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Verify that errors are generated when the vertex shader intput doesn't match the bound attribute
// types
TEST_P(WebGL2CompatibilityTest, VertexShaderAttributeTypeMissmatch)
{
    const std::string vertexShader =
        "#version 300 es\n"
        "in vec4 floatInput;\n"
        "in uvec4 uintInput;\n"
        "in ivec4 intInput;\n"
        "void main() {\n"
        "    gl_Position = vec4(floatInput.x, uintInput.x, intInput.x, 1);\n"
        "}\n";

    const std::string fragmentShader =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 outputColor;\n"
        "void main() {\n"
        "    outputColor = vec4(0, 0, 0, 1);"
        "}\n";

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    glUseProgram(program.get());

    GLint floatLocation = glGetAttribLocation(program, "floatInput");
    GLint uintLocation  = glGetAttribLocation(program, "uintInput");
    GLint intLocation   = glGetAttribLocation(program, "intInput");

    // Default attributes are of float types
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Set the default attributes to the correct types, should succeed
    glVertexAttribI4ui(uintLocation, 0, 0, 0, 1);
    glVertexAttribI4i(intLocation, 0, 0, 0, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Change the default float attribute to an integer, should fail
    glVertexAttribI4ui(floatLocation, 0, 0, 0, 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Use a buffer for some attributes
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);
    glEnableVertexAttribArray(floatLocation);
    glVertexAttribPointer(floatLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Use a float pointer attrib for a uint input
    glEnableVertexAttribArray(uintLocation);
    glVertexAttribPointer(uintLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Use a uint pointer for the uint input
    glVertexAttribIPointer(uintLocation, 4, GL_UNSIGNED_INT, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();
}

// Tests the WebGL removal of undefined behavior when attachments aren't written to.
TEST_P(WebGLCompatibilityTest, DrawBuffers)
{
    // Make sure we can use at least 4 attachments for the tests.
    bool useEXT = false;
    if (getClientMajorVersion() < 3)
    {
        if (!extensionRequestable("GL_EXT_draw_buffers"))
        {
            std::cout << "Test skipped because draw buffers are not available" << std::endl;
            return;
        }

        glRequestExtensionANGLE("GL_EXT_draw_buffers");
        useEXT = true;
        EXPECT_GL_NO_ERROR();
    }

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    if (maxDrawBuffers < 4)
    {
        std::cout << "Test skipped because MAX_DRAW_BUFFERS is too small." << std::endl;
        return;
    }

    // Clears all the renderbuffers to red.
    auto ClearEverythingToRed = [](GLRenderbuffer *renderbuffers) {
        GLFramebuffer clearFBO;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, clearFBO);

        glClearColor(1, 0, 0, 1);
        for (int i = 0; i < 4; ++i)
        {
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                      renderbuffers[i]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        ASSERT_GL_NO_ERROR();
    };

    // Checks that the renderbuffers specified by mask have the correct color
    auto CheckColors = [](GLRenderbuffer *renderbuffers, int mask, GLColor color) {
        GLFramebuffer readFBO;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);

        for (int i = 0; i < 4; ++i)
        {
            if (mask & (1 << i))
            {
                glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_RENDERBUFFER, renderbuffers[i]);
                EXPECT_PIXEL_COLOR_EQ(0, 0, color);
            }
        }
        ASSERT_GL_NO_ERROR();
    };

    // Depending on whether we are using the extension or ES3, a different entrypoint must be called
    auto DrawBuffers = [](bool useEXT, int numBuffers, GLenum *buffers) {
        if (useEXT)
        {
            glDrawBuffersEXT(numBuffers, buffers);
        }
        else
        {
            glDrawBuffers(numBuffers, buffers);
        }
    };

    // Initialized the test framebuffer
    GLFramebuffer drawFBO;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);

    GLRenderbuffer renderbuffers[4];
    for (int i = 0; i < 4; ++i)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER,
                                  renderbuffers[i]);
    }

    ASSERT_GL_NO_ERROR();

    const char *vertESSL1 =
        "attribute vec4 a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = a_pos;\n"
        "}\n";
    const char *vertESSL3 =
        "#version 300 es\n"
        "in vec4 a_pos;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = a_pos;\n"
        "}\n";

    GLenum allDrawBuffers[] = {
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
    };

    GLenum halfDrawBuffers[] = {
        GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
    };

    // Test that when using gl_FragColor, only the first attachment is written to.
    const char *fragESSL1 =
        "precision highp float;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}\n";
    ANGLE_GL_PROGRAM(programESSL1, vertESSL1, fragESSL1);

    {
        ClearEverythingToRed(renderbuffers);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
        DrawBuffers(useEXT, 4, allDrawBuffers);
        drawQuad(programESSL1, "a_pos", 0.5, 1.0, true);
        ASSERT_GL_NO_ERROR();

        CheckColors(renderbuffers, 0b0001, GLColor::green);
        CheckColors(renderbuffers, 0b1110, GLColor::red);
    }

    // Test that when using gl_FragColor, but the first draw buffer is 0, then no attachment is
    // written to.
    {
        ClearEverythingToRed(renderbuffers);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
        DrawBuffers(useEXT, 4, halfDrawBuffers);
        drawQuad(programESSL1, "a_pos", 0.5, 1.0, true);
        ASSERT_GL_NO_ERROR();

        CheckColors(renderbuffers, 0b1111, GLColor::red);
    }

    // Test what happens when rendering to a subset of the outputs. There is a behavior difference
    // between the extension and ES3. In the extension gl_FragData is implicitly declared as an
    // array of size MAX_DRAW_BUFFERS, so the WebGL spec stipulates that elements not written to
    // should default to 0. On the contrary, in ES3 outputs are specified one by one, so
    // attachments not declared in the shader should not be written to.
    const char *writeOddOutputsVert;
    const char *writeOddOutputsFrag;
    GLColor unwrittenColor;
    if (useEXT)
    {
        // In the extension, when an attachment isn't written to, it should get 0's
        unwrittenColor      = GLColor(0, 0, 0, 0);
        writeOddOutputsVert = vertESSL1;
        writeOddOutputsFrag =
            "#extension GL_EXT_draw_buffers : require\n"
            "precision highp float;\n"
            "void main()\n"
            "{\n"
            "    gl_FragData[1] = vec4(0.0, 1.0, 0.0, 1.0);\n"
            "    gl_FragData[3] = vec4(0.0, 1.0, 0.0, 1.0);\n"
            "}\n";
    }
    else
    {
        // In ES3 if an attachment isn't declared, it shouldn't get written and should be red
        // because of the preceding clears.
        unwrittenColor      = GLColor::red;
        writeOddOutputsVert = vertESSL3;
        writeOddOutputsFrag =
            "#version 300 es\n"
            "precision highp float;\n"
            "layout(location = 1) out vec4 output1;"
            "layout(location = 3) out vec4 output2;"
            "void main()\n"
            "{\n"
            "    output1 = vec4(0.0, 1.0, 0.0, 1.0);\n"
            "    output2 = vec4(0.0, 1.0, 0.0, 1.0);\n"
            "}\n";
    }
    ANGLE_GL_PROGRAM(writeOddOutputsProgram, writeOddOutputsVert, writeOddOutputsFrag);

    // Test that attachments not written to get the "unwritten" color
    {
        ClearEverythingToRed(renderbuffers);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
        DrawBuffers(useEXT, 4, allDrawBuffers);
        drawQuad(writeOddOutputsProgram, "a_pos", 0.5, 1.0, true);
        ASSERT_GL_NO_ERROR();

        CheckColors(renderbuffers, 0b1010, GLColor::green);
        CheckColors(renderbuffers, 0b0101, unwrittenColor);
    }

    // Test that attachments not written to get the "unwritten" color but that even when the
    // extension is used, disabled attachments are not written at all and stay red.
    {
        ClearEverythingToRed(renderbuffers);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
        DrawBuffers(useEXT, 4, halfDrawBuffers);
        drawQuad(writeOddOutputsProgram, "a_pos", 0.5, 1.0, true);
        ASSERT_GL_NO_ERROR();

        CheckColors(renderbuffers, 0b1000, GLColor::green);
        CheckColors(renderbuffers, 0b0100, unwrittenColor);
        CheckColors(renderbuffers, 0b0011, GLColor::red);
    }
}

// Linking should fail when corresponding vertex/fragment uniform blocks have different precision
// qualifiers.
TEST_P(WebGL2CompatibilityTest, UniformBlockPrecisionMismatch)
{
    const std::string vertexShader =
        "#version 300 es\n"
        "uniform Block { mediump vec4 val; };\n"
        "void main() { gl_Position = val; }\n";
    const std::string fragmentShader =
        "#version 300 es\n"
        "uniform Block { highp vec4 val; };\n"
        "out highp vec4 out_FragColor;\n"
        "void main() { out_FragColor = val; }\n";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
    ASSERT_NE(0u, vs);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);
    ASSERT_NE(0u, fs);

    GLuint program = glCreateProgram();

    glAttachShader(program, vs);
    glDeleteShader(vs);
    glAttachShader(program, fs);
    glDeleteShader(fs);

    glLinkProgram(program);
    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    ASSERT_EQ(0, linkStatus);

    glDeleteProgram(program);
}

// Test no attribute vertex shaders
TEST_P(WebGL2CompatibilityTest, NoAttributeVertexShader)
{
    const std::string vertexShader =
        "#version 300 es\n"
        "void main()\n"
        "{\n"
        "\n"
        "    ivec2 xy = ivec2(gl_VertexID % 2, (gl_VertexID / 2 + gl_VertexID / 3) % 2);\n"
        "    gl_Position = vec4(vec2(xy) * 2. - 1., 0, 1);\n"
        "}";
    const std::string fragmentShader =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 result;\n"
        "void main()\n"
        "{\n"
        "    result = vec4(0, 1, 0, 1);\n"
        "}";

    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    glUseProgram(program);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests bindAttribLocations for length limit
TEST_P(WebGL2CompatibilityTest, BindAttribLocationLimitation)
{
    constexpr int maxLocStringLength = 1024;
    const std::string tooLongString(maxLocStringLength + 1, '_');

    glBindAttribLocation(0, 0, static_cast<const GLchar *>(tooLongString.c_str()));

    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(WebGLCompatibilityTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());

ANGLE_INSTANTIATE_TEST(WebGL2CompatibilityTest, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());
}  // namespace
