//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramD3D.h: Defines the rx::ProgramD3D class which implements rx::ProgramImpl.

#ifndef LIBGLESV2_RENDERER_PROGRAMD3D_H_
#define LIBGLESV2_RENDERER_PROGRAMD3D_H_

#include "libGLESv2/renderer/ProgramImpl.h"
#include "libGLESv2/renderer/Workarounds.h"

#include <string>
#include <vector>

namespace gl
{
struct LinkedUniform;
struct VariableLocation;
struct VertexFormat;
}

namespace rx
{

class UniformStorage;

class ProgramD3D : public ProgramImpl
{
  public:
    ProgramD3D(rx::Renderer *renderer);
    virtual ~ProgramD3D();

    static ProgramD3D *makeProgramD3D(ProgramImpl *impl);
    static const ProgramD3D *makeProgramD3D(const ProgramImpl *impl);

    const std::vector<rx::PixelShaderOutputVariable> &getPixelShaderKey() { return mPixelShaderKey; }
    int getShaderVersion() const { return mShaderVersion; }
    GLenum getTransformFeedbackBufferMode() const { return mTransformFeedbackBufferMode; }
    std::vector<gl::LinkedVarying> &getTransformFeedbackLinkedVaryings() { return mTransformFeedbackLinkedVaryings; }
    sh::Attribute *getShaderAttributes() { return mShaderAttributes; }

    bool usesPointSize() const { return mUsesPointSize; }
    bool usesPointSpriteEmulation() const;
    bool usesGeometryShader() const;

    GLenum getBinaryFormat() { return GL_PROGRAM_BINARY_ANGLE; }
    gl::LinkResult load(gl::InfoLog &infoLog, gl::BinaryInputStream *stream);
    gl::Error save(gl::BinaryOutputStream *stream);

    gl::Error getPixelExecutableForFramebuffer(const gl::Framebuffer *fbo, ShaderExecutable **outExectuable);
    gl::Error getPixelExecutableForOutputLayout(const std::vector<GLenum> &outputLayout, ShaderExecutable **outExectuable);
    gl::Error getVertexExecutableForInputLayout(const gl::VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS], ShaderExecutable **outExectuable);
    ShaderExecutable *getGeometryExecutable() const { return mGeometryExecutable; }

    gl::LinkResult compileProgramExecutables(gl::InfoLog &infoLog, gl::Shader *fragmentShader, gl::Shader *vertexShader,
                                             int registers);

    gl::LinkResult link(gl::InfoLog &infoLog, gl::Shader *fragmentShader, gl::Shader *vertexShader,
                        const std::vector<std::string> &transformFeedbackVaryings, GLenum transformFeedbackBufferMode,
                        int *registers, std::vector<gl::LinkedVarying> *linkedVaryings,
                        std::map<int, gl::VariableLocation> *outputVariables, const gl::Caps &caps);

    void getInputLayoutSignature(const gl::VertexFormat inputLayout[], GLenum signature[]) const;

    void initializeUniformStorage(const std::vector<gl::LinkedUniform*> &uniforms);
    gl::Error applyUniforms(const std::vector<gl::LinkedUniform*> &uniforms);
    gl::Error applyUniformBuffers(const std::vector<gl::UniformBlock*> uniformBlocks, const std::vector<gl::Buffer*> boundBuffers,
                             const gl::Caps &caps);
    bool assignUniformBlockRegister(gl::InfoLog &infoLog, gl::UniformBlock *uniformBlock, GLenum shader,
                                    unsigned int registerIndex, const gl::Caps &caps);
    unsigned int getReservedUniformVectors(GLenum shader);

    const UniformStorage &getVertexUniformStorage() const { return *mVertexUniformStorage; }
    const UniformStorage &getFragmentUniformStorage() const { return *mFragmentUniformStorage; }

    void reset();

  private:
    DISALLOW_COPY_AND_ASSIGN(ProgramD3D);

    class VertexExecutable
    {
      public:
        VertexExecutable(const gl::VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS],
                         const GLenum signature[gl::MAX_VERTEX_ATTRIBS],
                         rx::ShaderExecutable *shaderExecutable);
        ~VertexExecutable();

        bool matchesSignature(const GLenum convertedLayout[gl::MAX_VERTEX_ATTRIBS]) const;

        const gl::VertexFormat *inputs() const { return mInputs; }
        const GLenum *signature() const { return mSignature; }
        rx::ShaderExecutable *shaderExecutable() const { return mShaderExecutable; }

      private:
        gl::VertexFormat mInputs[gl::MAX_VERTEX_ATTRIBS];
        GLenum mSignature[gl::MAX_VERTEX_ATTRIBS];
        rx::ShaderExecutable *mShaderExecutable;
    };

    class PixelExecutable
    {
      public:
        PixelExecutable(const std::vector<GLenum> &outputSignature, rx::ShaderExecutable *shaderExecutable);
        ~PixelExecutable();

        bool matchesSignature(const std::vector<GLenum> &signature) const { return mOutputSignature == signature; }

        const std::vector<GLenum> &outputSignature() const { return mOutputSignature; }
        rx::ShaderExecutable *shaderExecutable() const { return mShaderExecutable; }

      private:
        std::vector<GLenum> mOutputSignature;
        rx::ShaderExecutable *mShaderExecutable;
    };

    Renderer *mRenderer;
    DynamicHLSL *mDynamicHLSL;

    std::vector<VertexExecutable *> mVertexExecutables;
    std::vector<PixelExecutable *> mPixelExecutables;
    rx::ShaderExecutable *mGeometryExecutable;

    std::string mVertexHLSL;
    rx::D3DWorkaroundType mVertexWorkarounds;

    std::string mPixelHLSL;
    rx::D3DWorkaroundType mPixelWorkarounds;
    bool mUsesFragDepth;
    std::vector<rx::PixelShaderOutputVariable> mPixelShaderKey;

    bool mUsesPointSize;

    UniformStorage *mVertexUniformStorage;
    UniformStorage *mFragmentUniformStorage;

    GLenum mTransformFeedbackBufferMode;
    std::vector<gl::LinkedVarying> mTransformFeedbackLinkedVaryings;

    sh::Attribute mShaderAttributes[gl::MAX_VERTEX_ATTRIBS];

    int mShaderVersion;
};

}

#endif // LIBGLESV2_RENDERER_PROGRAMD3D_H_
