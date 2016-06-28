//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CHROMIUMPathRenderingTest
//   Test CHROMIUM subset of NV_path_rendering
//   This extension allows to render geometric paths as first class GL objects.

#include "test_utils/ANGLETest.h"
#include "shader_utils.h"

#include <cmath>
#include <cstring>
#include <cstddef>
#include <fstream>

using namespace angle;

namespace
{

bool CheckPixels(GLint x,
                 GLint y,
                 GLsizei width,
                 GLsizei height,
                 GLint tolerance,
                 const angle::GLColor &color)
{
    for (GLint yy = 0; yy < height; ++yy)
    {
        for (GLint xx = 0; xx < width; ++xx)
        {
            const auto px = x + xx;
            const auto py = y + yy;
            EXPECT_PIXEL_COLOR_EQ(px, py, color);
        }
    }

    return true;
}

void ExpectEqualMatrix(const GLfloat *expected, const GLfloat *actual)
{
    for (size_t i = 0; i < 16; ++i)
    {
        EXPECT_EQ(expected[i], actual[i]);
    }
}

void ExpectEqualMatrix(const GLfloat *expected, const GLint *actual)
{
    for (size_t i = 0; i < 16; ++i)
    {
        EXPECT_EQ(static_cast<GLint>(std::roundf(expected[i])), actual[i]);
    }
}

const int kResolution = 300;

class CHROMIUMPathRenderingTest : public ANGLETest
{
  protected:
    CHROMIUMPathRenderingTest()
    {
        setWindowWidth(kResolution);
        setWindowHeight(kResolution);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(8);
        setConfigStencilBits(8);
    }

    bool isApplicable() const { return extensionEnabled("GL_CHROMIUM_path_rendering"); }

    void tryAllDrawFunctions(GLuint path, GLenum err)
    {
        glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F);
        EXPECT_GL_ERROR(err);

        glStencilFillPathCHROMIUM(path, GL_COUNT_DOWN_CHROMIUM, 0x7F);
        EXPECT_GL_ERROR(err);

        glStencilStrokePathCHROMIUM(path, 0x80, 0x80);
        EXPECT_GL_ERROR(err);

        glCoverFillPathCHROMIUM(path, GL_BOUNDING_BOX_CHROMIUM);
        EXPECT_GL_ERROR(err);

        glCoverStrokePathCHROMIUM(path, GL_BOUNDING_BOX_CHROMIUM);
        EXPECT_GL_ERROR(err);

        glStencilThenCoverStrokePathCHROMIUM(path, 0x80, 0x80, GL_BOUNDING_BOX_CHROMIUM);
        EXPECT_GL_ERROR(err);

        glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F,
                                           GL_BOUNDING_BOX_CHROMIUM);
        EXPECT_GL_ERROR(err);
    }
};

// Test setting and getting of path rendering matrices.
TEST_P(CHROMIUMPathRenderingTest, TestMatrix)
{
    if (!isApplicable())
        return;

    static const GLfloat kIdentityMatrix[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                                0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    static const GLfloat kSeqMatrix[16] = {0.5f,   -0.5f,  -0.1f,  -0.8f, 4.4f,   5.5f,
                                           6.6f,   7.7f,   8.8f,   9.9f,  10.11f, 11.22f,
                                           12.33f, 13.44f, 14.55f, 15.66f};

    static const GLenum kMatrixModes[] = {GL_PATH_MODELVIEW_CHROMIUM, GL_PATH_PROJECTION_CHROMIUM};

    static const GLenum kGetMatrixModes[] = {GL_PATH_MODELVIEW_MATRIX_CHROMIUM,
                                             GL_PATH_PROJECTION_MATRIX_CHROMIUM};

    for (size_t i = 0; i < 2; ++i)
    {
        GLfloat mf[16];
        GLint mi[16];
        std::memset(mf, 0, sizeof(mf));
        std::memset(mi, 0, sizeof(mi));
        glGetFloatv(kGetMatrixModes[i], mf);
        glGetIntegerv(kGetMatrixModes[i], mi);
        ExpectEqualMatrix(kIdentityMatrix, mf);
        ExpectEqualMatrix(kIdentityMatrix, mi);

        glMatrixLoadfCHROMIUM(kMatrixModes[i], kSeqMatrix);
        std::memset(mf, 0, sizeof(mf));
        std::memset(mi, 0, sizeof(mi));
        glGetFloatv(kGetMatrixModes[i], mf);
        glGetIntegerv(kGetMatrixModes[i], mi);
        ExpectEqualMatrix(kSeqMatrix, mf);
        ExpectEqualMatrix(kSeqMatrix, mi);

        glMatrixLoadIdentityCHROMIUM(kMatrixModes[i]);
        std::memset(mf, 0, sizeof(mf));
        std::memset(mi, 0, sizeof(mi));
        glGetFloatv(kGetMatrixModes[i], mf);
        glGetIntegerv(kGetMatrixModes[i], mi);
        ExpectEqualMatrix(kIdentityMatrix, mf);
        ExpectEqualMatrix(kIdentityMatrix, mi);

        ASSERT_GL_NO_ERROR();
    }
}

// Test that trying to set incorrect matrix target results
// in a GL error.
TEST_P(CHROMIUMPathRenderingTest, TestMatrixErrors)
{
    if (!isApplicable())
        return;

    GLfloat mf[16];
    std::memset(mf, 0, sizeof(mf));

    glMatrixLoadfCHROMIUM(GL_PATH_MODELVIEW_CHROMIUM, mf);
    ASSERT_GL_NO_ERROR();

    glMatrixLoadIdentityCHROMIUM(GL_PATH_PROJECTION_CHROMIUM);
    ASSERT_GL_NO_ERROR();

    // Test that invalid matrix targets fail.
    glMatrixLoadfCHROMIUM(GL_PATH_MODELVIEW_CHROMIUM - 1, mf);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Test that invalid matrix targets fail.
    glMatrixLoadIdentityCHROMIUM(GL_PATH_PROJECTION_CHROMIUM + 1);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Test basic path create and delete.
TEST_P(CHROMIUMPathRenderingTest, TestGenDelete)
{
    if (!isApplicable())
        return;

    // This is unspecified in NV_path_rendering.
    EXPECT_EQ(0u, glGenPathsCHROMIUM(0));
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    GLuint path = glGenPathsCHROMIUM(1);
    EXPECT_NE(0u, path);
    glDeletePathsCHROMIUM(path, 1);
    ASSERT_GL_NO_ERROR();

    GLuint first_path = glGenPathsCHROMIUM(5);
    EXPECT_NE(0u, first_path);
    glDeletePathsCHROMIUM(first_path, 5);
    ASSERT_GL_NO_ERROR();

    // Test deleting paths that are not actually allocated:
    // "unused names in /paths/ are silently ignored".
    first_path = glGenPathsCHROMIUM(5);
    EXPECT_NE(0u, first_path);
    glDeletePathsCHROMIUM(first_path, 6);
    ASSERT_GL_NO_ERROR();

    GLsizei big_range = 0xffff;
    first_path = glGenPathsCHROMIUM(big_range);
    EXPECT_NE(0u, first_path);
    glDeletePathsCHROMIUM(first_path, big_range);
    ASSERT_GL_NO_ERROR();

    // Test glIsPathCHROMIUM(). A path object is not considered a path untill
    // it has actually been specified with a path data.

    path = glGenPathsCHROMIUM(1);
    EXPECT_TRUE(glIsPathCHROMIUM(path) == GL_FALSE);

    // specify the data.
    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[] = {50.0f, 50.0f};
    glPathCommandsCHROMIUM(path, 2, commands, 2, GL_FLOAT, coords);
    EXPECT_TRUE(glIsPathCHROMIUM(path) == GL_TRUE);
    glDeletePathsCHROMIUM(path, 1);
    EXPECT_TRUE(glIsPathCHROMIUM(path) == GL_FALSE);
}

// Test incorrect path creation and deletion and expect GL errors.
TEST_P(CHROMIUMPathRenderingTest, TestGenDeleteErrors)
{
    if (!isApplicable())
        return;

    // GenPaths / DeletePaths tests.
    // std::numeric_limits<GLuint>::max() is wrong for GLsizei.
    GLuint first_path = glGenPathsCHROMIUM(std::numeric_limits<GLuint>::max());
    EXPECT_EQ(first_path, 0u);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    first_path = glGenPathsCHROMIUM(-1);
    EXPECT_EQ(0u, first_path);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glDeletePathsCHROMIUM(1, -5);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    first_path = glGenPathsCHROMIUM(-1);
    EXPECT_EQ(0u, first_path);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test setting and getting path parameters.
TEST_P(CHROMIUMPathRenderingTest, TestPathParameter)
{
    if (!isApplicable())
        return;

    GLuint path = glGenPathsCHROMIUM(1);

    // specify the data.
    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[] = {50.0f, 50.0f};
    glPathCommandsCHROMIUM(path, 2, commands, 2, GL_FLOAT, coords);
    ASSERT_GL_NO_ERROR();
    EXPECT_TRUE(glIsPathCHROMIUM(path) == GL_TRUE);

    static const GLenum kEndCaps[] = {GL_FLAT_CHROMIUM, GL_SQUARE_CHROMIUM, GL_ROUND_CHROMIUM};
    for (std::size_t i = 0; i < 3; ++i)
    {
        GLint x;
        glPathParameteriCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, static_cast<GLenum>(kEndCaps[i]));
        ASSERT_GL_NO_ERROR();
        glGetPathParameterivCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, &x);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(kEndCaps[i], static_cast<GLenum>(x));

        GLfloat f;
        glPathParameterfCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM,
                                 static_cast<GLfloat>(kEndCaps[(i + 1) % 3]));
        glGetPathParameterfvCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, &f);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(kEndCaps[(i + 1) % 3], static_cast<GLenum>(f));
    }

    static const GLenum kJoinStyles[] = {GL_MITER_REVERT_CHROMIUM, GL_BEVEL_CHROMIUM,
                                         GL_ROUND_CHROMIUM};
    for (std::size_t i = 0; i < 3; ++i)
    {
        GLint x;
        glPathParameteriCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM,
                                 static_cast<GLenum>(kJoinStyles[i]));
        ASSERT_GL_NO_ERROR();
        glGetPathParameterivCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM, &x);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(kJoinStyles[i], static_cast<GLenum>(x));

        GLfloat f;
        glPathParameterfCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM,
                                 static_cast<GLfloat>(kJoinStyles[(i + 1) % 3]));
        ASSERT_GL_NO_ERROR();
        glGetPathParameterfvCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM, &f);
        ASSERT_GL_NO_ERROR();
        EXPECT_EQ(kJoinStyles[(i + 1) % 3], static_cast<GLenum>(f));
    }

    {
        glPathParameterfCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM, 5.0f);
        ASSERT_GL_NO_ERROR();

        GLfloat f;
        glGetPathParameterfvCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM, &f);
        EXPECT_EQ(5.0f, f);
    }

    glDeletePathsCHROMIUM(path, 1);
}

// Test that setting incorrect path parameter generates GL error.
TEST_P(CHROMIUMPathRenderingTest, TestPathParameterErrors)
{
    if (!isApplicable())
        return;

    GLuint path = glGenPathsCHROMIUM(1);

    // PathParameter*: Wrong value for the pname should fail.
    glPathParameteriCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM, GL_FLAT_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glPathParameterfCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, GL_MITER_REVERT_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // PathParameter*: Wrong floating-point value should fail.
    glPathParameterfCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM, -0.1f);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // PathParameter*: Wrong pname should fail.
    glPathParameteriCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM - 1, 5);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glDeletePathsCHROMIUM(path, 1);
}

// Test expected path object state.
TEST_P(CHROMIUMPathRenderingTest, TestPathObjectState)
{
    if (!isApplicable())
        return;

    glViewport(0, 0, kResolution, kResolution);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glStencilMask(0xffffffff);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    ASSERT_GL_NO_ERROR();

    // Test that trying to draw non-existing paths does not produce errors or results.
    GLuint non_existing_paths[] = {0, 55, 74744};
    for (auto &p : non_existing_paths)
    {
        EXPECT_TRUE(glIsPathCHROMIUM(p) == GL_FALSE);
        ASSERT_GL_NO_ERROR();
        tryAllDrawFunctions(p, GL_NO_ERROR);
    }

    // Path name marked as used but without path object state causes
    // a GL error upon any draw command.
    GLuint path = glGenPathsCHROMIUM(1);
    EXPECT_TRUE(glIsPathCHROMIUM(path) == GL_FALSE);
    tryAllDrawFunctions(path, GL_INVALID_OPERATION);
    glDeletePathsCHROMIUM(path, 1);

    // Document a bit of an inconsistency: path name marked as used but without
    // path object state causes a GL error upon any draw command (tested above).
    // Path name that had path object state, but then was "cleared", still has a
    // path object state, even though the state is empty.
    path = glGenPathsCHROMIUM(1);
    EXPECT_TRUE(glIsPathCHROMIUM(path) == GL_FALSE);

    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[] = {50.0f, 50.0f};
    glPathCommandsCHROMIUM(path, 2, commands, 2, GL_FLOAT, coords);
    EXPECT_TRUE(glIsPathCHROMIUM(path) == GL_TRUE);

    glPathCommandsCHROMIUM(path, 0, NULL, 0, GL_FLOAT, NULL);
    EXPECT_TRUE(glIsPathCHROMIUM(path) == GL_TRUE);  // The surprise.

    tryAllDrawFunctions(path, GL_NO_ERROR);
    glDeletePathsCHROMIUM(path, 1);

    // Make sure nothing got drawn by the drawing commands that should not produce
    // anything.
    const angle::GLColor black = {0, 0, 0, 0};
    EXPECT_TRUE(CheckPixels(0, 0, kResolution, kResolution, 0, black));
}

// Test that trying to use path object that doesn't exist generates
// a GL error.
TEST_P(CHROMIUMPathRenderingTest, TestUnnamedPathsErrors)
{
    if (!isApplicable())
        return;

    // Unnamed paths: Trying to create a path object with non-existing path name
    // produces error.  (Not a error in real NV_path_rendering).
    ASSERT_GL_NO_ERROR();
    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[] = {50.0f, 50.0f};
    glPathCommandsCHROMIUM(555, 2, commands, 2, GL_FLOAT, coords);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // PathParameter*: Using non-existing path object produces error.
    ASSERT_GL_NO_ERROR();
    glPathParameterfCHROMIUM(555, GL_PATH_STROKE_WIDTH_CHROMIUM, 5.0f);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    ASSERT_GL_NO_ERROR();
    glPathParameteriCHROMIUM(555, GL_PATH_JOIN_STYLE_CHROMIUM, GL_ROUND_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that setting incorrect path data generates a GL error.
TEST_P(CHROMIUMPathRenderingTest, TestPathCommandsErrors)
{
    if (!isApplicable())
        return;

    static const GLenum kInvalidCoordType = GL_NONE;

    GLuint path        = glGenPathsCHROMIUM(1);
    GLubyte commands[] = {GL_MOVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};
    GLfloat coords[]   = {50.0f, 50.0f};

    glPathCommandsCHROMIUM(path, 2, commands, -4, GL_FLOAT, coords);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glPathCommandsCHROMIUM(path, -1, commands, 2, GL_FLOAT, coords);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glPathCommandsCHROMIUM(path, 2, commands, 2, kInvalidCoordType, coords);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // incorrect number of coordinates
    glPathCommandsCHROMIUM(path, 2, commands, std::numeric_limits<GLsizei>::max(), GL_FLOAT,
                           coords);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // This should fail due to cmd count + coord count * short size.
    glPathCommandsCHROMIUM(path, 2, commands, std::numeric_limits<GLsizei>::max(), GL_SHORT,
                           coords);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glDeletePathsCHROMIUM(path, 1);
}

// Test that trying to render a path with invalid arguments
// generates a GL error.
TEST_P(CHROMIUMPathRenderingTest, TestPathRenderingInvalidArgs)
{
    if (!isApplicable())
        return;

    GLuint path = glGenPathsCHROMIUM(1);
    glPathCommandsCHROMIUM(path, 0, NULL, 0, GL_FLOAT, NULL);

    // Verify that normal calls work.
    glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F);
    ASSERT_GL_NO_ERROR();
    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F, GL_BOUNDING_BOX_CHROMIUM);
    ASSERT_GL_NO_ERROR();

    // Using invalid fill mode causes INVALID_ENUM.
    glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM - 1, 0x7F);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM - 1, 0x7F,
                                       GL_BOUNDING_BOX_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Using invalid cover mode causes INVALID_ENUM.
    glCoverFillPathCHROMIUM(path, GL_CONVEX_HULL_CHROMIUM - 1);
    EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), glGetError());
    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F,
                                       GL_BOUNDING_BOX_CHROMIUM + 1);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // For instanced variants, we need this to error the same way
    // regardless of whether # of paths == 0 would cause an early return.

    // TODO: enable this once instanced path rendering is implemented.
    // for (int path_count = 0; path_count <= 1; ++path_count)
    // {
    //     glStencilFillPathInstancedCHROMIUM(path_count, GL_UNSIGNED_INT, &path, 0,
    //         GL_COUNT_UP_CHROMIUM - 1, 0x7F, GL_NONE, NULL);
    //     EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), glGetError());
    //     glStencilThenCoverFillPathInstancedCHROMIUM(
    //         path_count, GL_UNSIGNED_INT, &path, 0, GL_COUNT_UP_CHROMIUM - 1, 0x7F,
    //         GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM, GL_NONE, NULL);
    //     EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), glGetError());
    // }

    // Using mask+1 not being power of two causes INVALID_VALUE with up/down fill mode
    glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x40);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_DOWN_CHROMIUM, 12, GL_BOUNDING_BOX_CHROMIUM);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // TODO: enable this once instanced path rendering is implemented.
    // for (int path_count = 0; path_count <= 1; ++path_count)
    // {
    //     glStencilFillPathInstancedCHROMIUM(path_count, GL_UNSIGNED_INT, &path, 0,
    //         GL_COUNT_UP_CHROMIUM, 0x30, GL_NONE, NULL);
    //     EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), glGetError());
    //     glStencilThenCoverFillPathInstancedCHROMIUM(
    //         path_count, GL_UNSIGNED_INT, &path, 0, GL_COUNT_DOWN_CHROMIUM, 0xFE,
    //         GL_BOUNDING_BOX_OF_BOUNDING_BOXES_CHROMIUM, GL_NONE, NULL);
    //     EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), glGetError());
    // }

    glDeletePathsCHROMIUM(path, 1);
}

const GLfloat kProjectionMatrix[16] = {2.0f / kResolution,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       2.0f / kResolution,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       0.0f,
                                       -1.0f,
                                       0.0f,
                                       -1.0f,
                                       -1.0f,
                                       0.0f,
                                       1.0f};

class CHROMIUMPathRenderingDrawTest : public ANGLETest
{
  protected:
    CHROMIUMPathRenderingDrawTest()
    {
        setWindowWidth(kResolution);
        setWindowHeight(kResolution);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(8);
        setConfigStencilBits(8);
    }

    bool isApplicable() const { return extensionEnabled("GL_CHROMIUM_path_rendering"); }

    void setupStateForTestPattern()
    {
        glViewport(0, 0, kResolution, kResolution);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glStencilMask(0xffffffff);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_STENCIL_TEST);

        static const char *kVertexShaderSource =
            "void main() {\n"
            "  gl_Position = vec4(1.0); \n"
            "}";

        static const char *kFragmentShaderSource =
            "precision mediump float;\n"
            "uniform vec4 color;\n"
            "void main() {\n"
            "  gl_FragColor = color;\n"
            "}";

        GLuint program = CompileProgram(kVertexShaderSource, kFragmentShaderSource);
        glUseProgram(program);
        mColorLoc = glGetUniformLocation(program, "color");
        glDeleteProgram(program);

        // Set up orthogonal projection with near/far plane distance of 2.
        glMatrixLoadfCHROMIUM(GL_PATH_PROJECTION_CHROMIUM, kProjectionMatrix);
        glMatrixLoadIdentityCHROMIUM(GL_PATH_MODELVIEW_CHROMIUM);

        ASSERT_GL_NO_ERROR();
    }

    void setupPathStateForTestPattern(GLuint path)
    {
        static const GLubyte kCommands[] = {GL_MOVE_TO_CHROMIUM, GL_LINE_TO_CHROMIUM,
                                            GL_QUADRATIC_CURVE_TO_CHROMIUM,
                                            GL_CUBIC_CURVE_TO_CHROMIUM, GL_CLOSE_PATH_CHROMIUM};

        static const GLfloat kCoords[] = {50.0f, 50.0f, 75.0f, 75.0f, 100.0f, 62.5f, 50.0f,
                                          25.5f, 0.0f,  62.5f, 50.0f, 50.0f,  25.0f, 75.0f};

        glPathCommandsCHROMIUM(path, 5, kCommands, 14, GL_FLOAT, kCoords);
        glPathParameterfCHROMIUM(path, GL_PATH_STROKE_WIDTH_CHROMIUM, 5.0f);
        glPathParameterfCHROMIUM(path, GL_PATH_MITER_LIMIT_CHROMIUM, 1.0f);
        glPathParameterfCHROMIUM(path, GL_PATH_STROKE_BOUND_CHROMIUM, .02f);
        glPathParameteriCHROMIUM(path, GL_PATH_JOIN_STYLE_CHROMIUM, GL_ROUND_CHROMIUM);
        glPathParameteriCHROMIUM(path, GL_PATH_END_CAPS_CHROMIUM, GL_SQUARE_CHROMIUM);
        ASSERT_GL_NO_ERROR();
    }

    void verifyTestPatternFill(GLint x, GLint y)
    {
        static const GLint kFillCoords[]  = {55, 54, 50, 28, 66, 63};
        static const angle::GLColor kBlue = {0, 0, 255, 255};

        for (size_t i = 0; i < 6; i += 2)
        {
            GLint fx = kFillCoords[i];
            GLint fy = kFillCoords[i + 1];
            EXPECT_TRUE(CheckPixels(x + fx, y + fy, 1, 1, 0, kBlue));
        }
    }
    void verifyTestPatternBg(GLint x, GLint y)
    {
        static const GLint kBackgroundCoords[]     = {80, 80, 20, 20, 90, 1};
        static const angle::GLColor kExpectedColor = {0, 0, 0, 0};

        for (size_t i = 0; i < 6; i += 2)
        {
            GLint bx = kBackgroundCoords[i];
            GLint by = kBackgroundCoords[i + 1];
            EXPECT_TRUE(CheckPixels(x + bx, y + by, 1, 1, 0, kExpectedColor));
        }
    }

    void verifyTestPatternStroke(GLint x, GLint y)
    {
        // Inside the stroke we should have green.
        static const angle::GLColor kGreen = {0, 255, 0, 255};
        EXPECT_TRUE(CheckPixels(x + 50, y + 53, 1, 1, 0, kGreen));
        EXPECT_TRUE(CheckPixels(x + 26, y + 76, 1, 1, 0, kGreen));

        // Outside the path we should have black.
        static const angle::GLColor black = {0, 0, 0, 0};
        EXPECT_TRUE(CheckPixels(x + 10, y + 10, 1, 1, 0, black));
        EXPECT_TRUE(CheckPixels(x + 80, y + 80, 1, 1, 0, black));
    }

    GLuint mColorLoc;
};

// Tests that basic path rendering functions work.
TEST_P(CHROMIUMPathRenderingDrawTest, TestPathRendering)
{
    if (!isApplicable())
        return;

    static const float kBlue[]  = {0.0f, 0.0f, 1.0f, 1.0f};
    static const float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};

    setupStateForTestPattern();

    GLuint path = glGenPathsCHROMIUM(1);
    setupPathStateForTestPattern(path);

    // Do the stencil fill, cover fill, stencil stroke, cover stroke
    // in unconventional order:
    // 1) stencil the stroke in stencil high bit
    // 2) stencil the fill in low bits
    // 3) cover the fill
    // 4) cover the stroke
    // This is done to check that glPathStencilFunc works, eg the mask
    // goes through. Stencil func is not tested ATM, for simplicity.

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0xFF);
    glStencilStrokePathCHROMIUM(path, 0x80, 0x80);

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0x7F);
    glStencilFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F);

    glStencilFunc(GL_LESS, 0, 0x7F);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kBlue);
    glCoverFillPathCHROMIUM(path, GL_BOUNDING_BOX_CHROMIUM);

    glStencilFunc(GL_EQUAL, 0x80, 0x80);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kGreen);
    glCoverStrokePathCHROMIUM(path, GL_CONVEX_HULL_CHROMIUM);

    glDeletePathsCHROMIUM(path, 1);

    ASSERT_GL_NO_ERROR();

    // Verify the image.
    verifyTestPatternFill(0, 0);
    verifyTestPatternBg(0, 0);
    verifyTestPatternStroke(0, 0);
}

// Test that StencilThen{Stroke,Fill} path rendering functions work
TEST_P(CHROMIUMPathRenderingDrawTest, TestPathRenderingThenFunctions)
{
    if (!isApplicable())
        return;

    static float kBlue[]  = {0.0f, 0.0f, 1.0f, 1.0f};
    static float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};

    setupStateForTestPattern();

    GLuint path = glGenPathsCHROMIUM(1);
    setupPathStateForTestPattern(path);

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0xFF);
    glStencilFunc(GL_EQUAL, 0x80, 0x80);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kGreen);
    glStencilThenCoverStrokePathCHROMIUM(path, 0x80, 0x80, GL_BOUNDING_BOX_CHROMIUM);

    glPathStencilFuncCHROMIUM(GL_ALWAYS, 0, 0x7F);
    glStencilFunc(GL_LESS, 0, 0x7F);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glUniform4fv(mColorLoc, 1, kBlue);
    glStencilThenCoverFillPathCHROMIUM(path, GL_COUNT_UP_CHROMIUM, 0x7F, GL_CONVEX_HULL_CHROMIUM);

    glDeletePathsCHROMIUM(path, 1);

    // Verify the image.
    verifyTestPatternFill(0, 0);
    verifyTestPatternBg(0, 0);
    verifyTestPatternStroke(0, 0);
}

}  // namespace

ANGLE_INSTANTIATE_TEST(CHROMIUMPathRenderingTest,
                       ES2_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGL(),
                       ES3_OPENGLES());
ANGLE_INSTANTIATE_TEST(CHROMIUMPathRenderingDrawTest,
                       ES2_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGL(),
                       ES3_OPENGLES());
