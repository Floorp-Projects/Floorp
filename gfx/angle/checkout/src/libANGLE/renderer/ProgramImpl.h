//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramImpl.h: Defines the abstract rx::ProgramImpl class.

#ifndef LIBANGLE_RENDERER_PROGRAMIMPL_H_
#define LIBANGLE_RENDERER_PROGRAMIMPL_H_

#include "common/angleutils.h"
#include "libANGLE/BinaryStream.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Program.h"
#include "libANGLE/Shader.h"

#include <map>

namespace gl
{
class Context;
struct ProgramLinkedResources;
}

namespace sh
{
struct BlockMemberInfo;
}

namespace rx
{

// Provides a mechanism to access the result of asynchronous linking.
class LinkEvent : angle::NonCopyable
{
  public:
    virtual ~LinkEvent(){};

    // Please be aware that these methods may be called under a gl::Context other
    // than the one where the LinkEvent was created.
    //
    // Waits until the linking is actually done. Returns true if the linking
    // succeeded, false otherwise.
    virtual bool wait() = 0;
    // Peeks whether the linking is still ongoing.
    virtual bool isLinking() = 0;
};

// Wraps an already done linking.
class LinkEventDone final : public LinkEvent
{
  public:
    LinkEventDone(const gl::LinkResult &result) : mResult(result) {}
    bool wait() override { return (!mResult.isError() && mResult.getResult()); }
    bool isLinking() override { return false; }

  private:
    gl::LinkResult mResult;
};

class ProgramImpl : angle::NonCopyable
{
  public:
    ProgramImpl(const gl::ProgramState &state) : mState(state) {}
    virtual ~ProgramImpl() {}
    virtual gl::Error destroy(const gl::Context *context) { return gl::NoError(); }

    virtual gl::LinkResult load(const gl::Context *context,
                                gl::InfoLog &infoLog,
                                gl::BinaryInputStream *stream) = 0;
    virtual void save(const gl::Context *context, gl::BinaryOutputStream *stream) = 0;
    virtual void setBinaryRetrievableHint(bool retrievable) = 0;
    virtual void setSeparable(bool separable)               = 0;

    virtual std::unique_ptr<LinkEvent> link(const gl::Context *context,
                                            const gl::ProgramLinkedResources &resources,
                                            gl::InfoLog &infoLog)          = 0;
    virtual GLboolean validate(const gl::Caps &caps, gl::InfoLog *infoLog) = 0;

    virtual void setUniform1fv(GLint location, GLsizei count, const GLfloat *v) = 0;
    virtual void setUniform2fv(GLint location, GLsizei count, const GLfloat *v) = 0;
    virtual void setUniform3fv(GLint location, GLsizei count, const GLfloat *v) = 0;
    virtual void setUniform4fv(GLint location, GLsizei count, const GLfloat *v) = 0;
    virtual void setUniform1iv(GLint location, GLsizei count, const GLint *v) = 0;
    virtual void setUniform2iv(GLint location, GLsizei count, const GLint *v) = 0;
    virtual void setUniform3iv(GLint location, GLsizei count, const GLint *v) = 0;
    virtual void setUniform4iv(GLint location, GLsizei count, const GLint *v) = 0;
    virtual void setUniform1uiv(GLint location, GLsizei count, const GLuint *v) = 0;
    virtual void setUniform2uiv(GLint location, GLsizei count, const GLuint *v) = 0;
    virtual void setUniform3uiv(GLint location, GLsizei count, const GLuint *v) = 0;
    virtual void setUniform4uiv(GLint location, GLsizei count, const GLuint *v) = 0;
    virtual void setUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;
    virtual void setUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) = 0;

    // Done in the back-end to avoid having to keep a system copy of uniform data.
    virtual void getUniformfv(const gl::Context *context,
                              GLint location,
                              GLfloat *params) const = 0;
    virtual void getUniformiv(const gl::Context *context, GLint location, GLint *params) const = 0;
    virtual void getUniformuiv(const gl::Context *context,
                               GLint location,
                               GLuint *params) const = 0;

    // CHROMIUM_path_rendering
    // Set parameters to control fragment shader input variable interpolation
    virtual void setPathFragmentInputGen(const std::string &inputName,
                                         GLenum genMode,
                                         GLint components,
                                         const GLfloat *coeffs) = 0;

    // Implementation-specific method for ignoring unreferenced uniforms. Some implementations may
    // perform more extensive analysis and ignore some locations that ANGLE doesn't detect as
    // unreferenced. This method is not required to be overriden by a back-end.
    virtual void markUnusedUniformLocations(std::vector<gl::VariableLocation> *uniformLocations,
                                            std::vector<gl::SamplerBinding> *samplerBindings,
                                            std::vector<gl::ImageBinding> *imageBindings)
    {
    }

    const gl::ProgramState &getState() const { return mState; }

    virtual gl::Error syncState(const gl::Context *context,
                                const gl::Program::DirtyBits &dirtyBits);

  protected:
    const gl::ProgramState &mState;
};

inline gl::Error ProgramImpl::syncState(const gl::Context *context,
                                        const gl::Program::DirtyBits &dirtyBits)
{
    return gl::NoError();
}

}  // namespace rx

#endif // LIBANGLE_RENDERER_PROGRAMIMPL_H_
