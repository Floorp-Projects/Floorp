//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramImpl.h: Defines the abstract rx::ProgramImpl class.

#ifndef LIBGLESV2_RENDERER_PROGRAMIMPL_H_
#define LIBGLESV2_RENDERER_PROGRAMIMPL_H_

#include "common/angleutils.h"
#include "libGLESv2/BinaryStream.h"
#include "libGLESv2/Constants.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/renderer/Renderer.h"

namespace rx
{

class Renderer;

class ProgramImpl
{
public:
    virtual ~ProgramImpl() { }

    virtual bool usesPointSize() const = 0;
    virtual int getShaderVersion() const = 0;
    virtual GLenum getTransformFeedbackBufferMode() const = 0;
    virtual std::vector<gl::LinkedVarying> &getTransformFeedbackLinkedVaryings() = 0;
    virtual sh::Attribute *getShaderAttributes() = 0;

    virtual GLenum getBinaryFormat() = 0;
    virtual gl::LinkResult load(gl::InfoLog &infoLog, gl::BinaryInputStream *stream) = 0;
    virtual gl::Error save(gl::BinaryOutputStream *stream) = 0;

    virtual gl::LinkResult compileProgramExecutables(gl::InfoLog &infoLog, gl::Shader *fragmentShader, gl::Shader *vertexShader,
                                                     int registers) = 0;

    virtual gl::LinkResult link(gl::InfoLog &infoLog, gl::Shader *fragmentShader, gl::Shader *vertexShader,
                                const std::vector<std::string> &transformFeedbackVaryings, GLenum transformFeedbackBufferMode,
                                int *registers, std::vector<gl::LinkedVarying> *linkedVaryings,
                                std::map<int, gl::VariableLocation> *outputVariables, const gl::Caps &caps) = 0;

    virtual void initializeUniformStorage(const std::vector<gl::LinkedUniform*> &uniforms) = 0;

    virtual gl::Error applyUniforms(const std::vector<gl::LinkedUniform*> &uniforms) = 0;
    virtual gl::Error applyUniformBuffers(const std::vector<gl::UniformBlock*> uniformBlocks, const std::vector<gl::Buffer*> boundBuffers,
                                     const gl::Caps &caps) = 0;
    virtual bool assignUniformBlockRegister(gl::InfoLog &infoLog, gl::UniformBlock *uniformBlock, GLenum shader,
                                            unsigned int registerIndex, const gl::Caps &caps) = 0;
    virtual unsigned int getReservedUniformVectors(GLenum shader) = 0;

    virtual void reset() = 0;
};

}

#endif // LIBGLESV2_RENDERER_PROGRAMIMPL_H_
