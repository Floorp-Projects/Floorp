//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GLES1Renderer.h: Defines GLES1 emulation rendering operations on top of a GLES3
// context. Used by Context.h.

#ifndef LIBANGLE_GLES1_RENDERER_H_
#define LIBANGLE_GLES1_RENDERER_H_

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/angletypes.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace gl
{

class Context;
class Program;
class State;
class Shader;
class ShaderProgramManager;

class GLES1Renderer final : angle::NonCopyable
{
  public:
    GLES1Renderer();
    ~GLES1Renderer();

    void onDestroy(Context *context, State *state);

    Error prepareForDraw(Context *context, State *glState);

    int vertexArrayIndex(ClientVertexArrayType type, const State *glState) const;
    static int TexCoordArrayIndex(unsigned int unit);
    AttributesMask getVertexArraysAttributeMask(const State *glState) const;

  private:
    using Mat4Uniform = float[16];

    Shader *getShader(GLuint handle) const;
    Program *getProgram(GLuint handle) const;

    Error compileShader(Context *context,
                        ShaderType shaderType,
                        const char *src,
                        GLuint *shaderOut);
    Error linkProgram(Context *context,
                      State *glState,
                      GLuint vshader,
                      GLuint fshader,
                      const std::unordered_map<GLint, std::string> &attribLocs,
                      GLuint *programOut);
    Error initializeRendererProgram(Context *context, State *glState);

    static constexpr int kTexUnitCount = 4;

    static constexpr int kVertexAttribIndex           = 0;
    static constexpr int kNormalAttribIndex           = 1;
    static constexpr int kColorAttribIndex            = 2;
    static constexpr int kPointSizeAttribIndex        = 3;
    static constexpr int kTextureCoordAttribIndexBase = 4;

    bool mRendererProgramInitialized;
    ShaderProgramManager *mShaderPrograms;

    struct GLES1ProgramState
    {
        GLuint program;

        GLint projMatrixLoc;
        GLint modelviewMatrixLoc;
        GLint textureMatrixLoc;
        GLint modelviewInvTrLoc;

        std::array<GLint, kTexUnitCount> tex2DSamplerLocs;
        std::array<GLint, kTexUnitCount> texCubeSamplerLocs;

        GLint shadeModelFlatLoc;

        GLint enableTexture2DLoc;
        GLint enableTextureCubeMapLoc;
    };

    struct GLES1UniformBuffers
    {
        std::array<Mat4Uniform, kTexUnitCount> textureMatrices;
        std::array<GLint, kTexUnitCount> tex2DEnables;
        std::array<GLint, kTexUnitCount> texCubeEnables;
    };

    GLES1UniformBuffers mUniformBuffers;
    GLES1ProgramState mProgramState;
};

}  // namespace gl

#endif  // LIBANGLE_GLES1_RENDERER_H_
