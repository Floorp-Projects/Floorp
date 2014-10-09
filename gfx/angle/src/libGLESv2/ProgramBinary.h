//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.h: Defines the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#ifndef LIBGLESV2_PROGRAM_BINARY_H_
#define LIBGLESV2_PROGRAM_BINARY_H_

#include "common/RefCountObject.h"
#include "angletypes.h"
#include "common/mathutil.h"
#include "libGLESv2/Uniform.h"
#include "libGLESv2/Shader.h"
#include "libGLESv2/Constants.h"
#include "libGLESv2/renderer/d3d/VertexDataManager.h"
#include "libGLESv2/renderer/d3d/DynamicHLSL.h"

#include "angle_gl.h"

#include <string>
#include <vector>

namespace sh
{
class HLSLBlockEncoder;
}

#include <GLES3/gl3.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <string>
#include <vector>

namespace rx
{
class ShaderExecutable;
class Renderer;
struct TranslatedAttribute;
class UniformStorage;
class ProgramImpl;
}

namespace gl
{
struct Caps;
class Shader;
class InfoLog;
class AttributeBindings;
class Buffer;
class Framebuffer;

// Struct used for correlating uniforms/elements of uniform arrays to handles
struct VariableLocation
{
    VariableLocation()
    {
    }

    VariableLocation(const std::string &name, unsigned int element, unsigned int index);

    std::string name;
    unsigned int element;
    unsigned int index;
};

struct LinkedVarying
{
    LinkedVarying();
    LinkedVarying(const std::string &name, GLenum type, GLsizei size, const std::string &semanticName,
                  unsigned int semanticIndex, unsigned int semanticIndexCount);

    // Original GL name
    std::string name;

    GLenum type;
    GLsizei size;

    // DirectX semantic information
    std::string semanticName;
    unsigned int semanticIndex;
    unsigned int semanticIndexCount;
};

// This is the result of linking a program. It is the state that would be passed to ProgramBinary.
class ProgramBinary : public RefCountObject
{
  public:
    explicit ProgramBinary(rx::ProgramImpl *impl);
    ~ProgramBinary();

    rx::ProgramImpl *getImplementation() { return mProgram; }
    const rx::ProgramImpl *getImplementation() const { return mProgram; }

    GLuint getAttributeLocation(const char *name);
    int getSemanticIndex(int attributeIndex);

    GLint getSamplerMapping(SamplerType type, unsigned int samplerIndex, const Caps &caps);
    GLenum getSamplerTextureType(SamplerType type, unsigned int samplerIndex);
    GLint getUsedSamplerRange(SamplerType type);
    bool usesPointSize() const;

    GLint getUniformLocation(std::string name);
    GLuint getUniformIndex(std::string name);
    GLuint getUniformBlockIndex(std::string name);
    void setUniform1fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform2fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform3fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform4fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform1iv(GLint location, GLsizei count, const GLint *v);
    void setUniform2iv(GLint location, GLsizei count, const GLint *v);
    void setUniform3iv(GLint location, GLsizei count, const GLint *v);
    void setUniform4iv(GLint location, GLsizei count, const GLint *v);
    void setUniform1uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform2uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform3uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniform4uiv(GLint location, GLsizei count, const GLuint *v);
    void setUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void setUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

    void getUniformfv(GLint location, GLfloat *params);
    void getUniformiv(GLint location, GLint *params);
    void getUniformuiv(GLint location, GLuint *params);

    void dirtyAllUniforms();

    Error applyUniforms();
    Error applyUniformBuffers(const std::vector<Buffer*> boundBuffers, const Caps &caps);

    bool load(InfoLog &infoLog, GLenum binaryFormat, const void *binary, GLsizei length);
    bool save(GLenum *binaryFormat, void *binary, GLsizei bufSize, GLsizei *length);
    GLint getLength();

    bool link(InfoLog &infoLog, const AttributeBindings &attributeBindings, Shader *fragmentShader, Shader *vertexShader,
              const std::vector<std::string>& transformFeedbackVaryings, GLenum transformFeedbackBufferMode, const Caps &caps);
    void getAttachedShaders(GLsizei maxCount, GLsizei *count, GLuint *shaders);

    void getActiveAttribute(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) const;
    GLint getActiveAttributeCount() const;
    GLint getActiveAttributeMaxLength() const;

    void getActiveUniform(GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) const;
    GLint getActiveUniformCount() const;
    GLint getActiveUniformMaxLength() const;
    GLint getActiveUniformi(GLuint index, GLenum pname) const;
    bool isValidUniformLocation(GLint location) const;
    LinkedUniform *getUniformByLocation(GLint location) const;
    LinkedUniform *getUniformByName(const std::string &name) const;

    void getActiveUniformBlockName(GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName) const;
    void getActiveUniformBlockiv(GLuint uniformBlockIndex, GLenum pname, GLint *params) const;
    GLuint getActiveUniformBlockCount() const;
    GLuint getActiveUniformBlockMaxLength() const;
    UniformBlock *getUniformBlockByIndex(GLuint blockIndex);

    GLint getFragDataLocation(const char *name) const;

    size_t getTransformFeedbackVaryingCount() const;
    const LinkedVarying &getTransformFeedbackVarying(size_t idx) const;
    GLenum getTransformFeedbackBufferMode() const;

    void validate(InfoLog &infoLog, const Caps &caps);
    bool validateSamplers(InfoLog *infoLog, const Caps &caps);
    bool isValidated() const;
    void updateSamplerMapping();

    unsigned int getSerial() const;

    void initAttributesByLayout();
    void sortAttributesByLayout(rx::TranslatedAttribute attributes[MAX_VERTEX_ATTRIBS], int sortedSemanticIndices[MAX_VERTEX_ATTRIBS]) const;

    const std::vector<LinkedUniform*> &getUniforms() const { return mUniforms; }

    static bool linkVaryings(InfoLog &infoLog, Shader *fragmentShader, Shader *vertexShader);

  private:
    DISALLOW_COPY_AND_ASSIGN(ProgramBinary);

    struct Sampler
    {
        Sampler();

        bool active;
        GLint logicalTextureUnit;
        GLenum textureType;
    };

    void reset();

    bool linkAttributes(InfoLog &infoLog, const AttributeBindings &attributeBindings, const Shader *vertexShader);

    static bool linkValidateVariablesBase(InfoLog &infoLog,
                                          const std::string &variableName,
                                          const sh::ShaderVariable &vertexVariable,
                                          const sh::ShaderVariable &fragmentVariable,
                                          bool validatePrecision);

    static bool linkValidateUniforms(InfoLog &infoLog, const std::string &uniformName, const sh::Uniform &vertexUniform, const sh::Uniform &fragmentUniform);
    static bool linkValidateVaryings(InfoLog &infoLog, const std::string &varyingName, const sh::Varying &vertexVarying, const sh::Varying &fragmentVarying);
    static bool linkValidateInterfaceBlockFields(InfoLog &infoLog, const std::string &uniformName, const sh::InterfaceBlockField &vertexUniform, const sh::InterfaceBlockField &fragmentUniform);
    bool linkUniforms(InfoLog &infoLog, const Shader &vertexShader, const Shader &fragmentShader, const Caps &caps);
    void defineUniformBase(GLenum shader, const sh::Uniform &uniform, unsigned int uniformRegister);
    void defineUniform(GLenum shader, const sh::ShaderVariable &uniform, const std::string &fullName, sh::HLSLBlockEncoder *encoder);
    bool indexSamplerUniform(const LinkedUniform &uniform, InfoLog &infoLog, const Caps &caps);
    bool indexUniforms(InfoLog &infoLog, const Caps &caps);
    static bool assignSamplers(unsigned int startSamplerIndex, GLenum samplerType, unsigned int samplerCount,
                               std::vector<Sampler> &outSamplers, GLuint *outUsedRange);
    bool areMatchingInterfaceBlocks(InfoLog &infoLog, const sh::InterfaceBlock &vertexInterfaceBlock, const sh::InterfaceBlock &fragmentInterfaceBlock);
    bool linkUniformBlocks(InfoLog &infoLog, const Shader &vertexShader, const Shader &fragmentShader, const Caps &caps);
    bool gatherTransformFeedbackLinkedVaryings(InfoLog &infoLog, const std::vector<LinkedVarying> &linkedVaryings,
                                               const std::vector<std::string> &transformFeedbackVaryingNames,
                                               GLenum transformFeedbackBufferMode,
                                               std::vector<LinkedVarying> *outTransformFeedbackLinkedVaryings,
                                               const Caps &caps) const;
    template <typename VarT>
    void defineUniformBlockMembers(const std::vector<VarT> &fields, const std::string &prefix, int blockIndex,
                                   sh::BlockLayoutEncoder *encoder, std::vector<unsigned int> *blockUniformIndexes,
                                   bool inRowMajorLayout);
    bool defineUniformBlock(InfoLog &infoLog, const Shader &shader, const sh::InterfaceBlock &interfaceBlock, const Caps &caps);
    bool assignUniformBlockRegister(InfoLog &infoLog, UniformBlock *uniformBlock, GLenum shader, unsigned int registerIndex, const Caps &caps);
    void defineOutputVariables(Shader *fragmentShader);

    template <typename T>
    void setUniform(GLint location, GLsizei count, const T* v, GLenum targetUniformType);

    template <int cols, int rows>
    void setUniformMatrixfv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, GLenum targetUniformType);

    template <typename T>
    void getUniformv(GLint location, T *params, GLenum uniformType);

    rx::ProgramImpl *mProgram;

    sh::Attribute mLinkedAttribute[MAX_VERTEX_ATTRIBS];
    int mSemanticIndex[MAX_VERTEX_ATTRIBS];
    int mAttributesByLayout[MAX_VERTEX_ATTRIBS];

    std::vector<Sampler> mSamplersPS;
    std::vector<Sampler> mSamplersVS;
    GLuint mUsedVertexSamplerRange;
    GLuint mUsedPixelSamplerRange;
    bool mDirtySamplerMapping;

    std::vector<LinkedUniform*> mUniforms;
    std::vector<UniformBlock*> mUniformBlocks;
    std::vector<VariableLocation> mUniformIndex;
    std::map<int, VariableLocation> mOutputVariables;

    bool mValidated;

    const unsigned int mSerial;

    static unsigned int issueSerial();
    static unsigned int mCurrentSerial;
};

}

#endif   // LIBGLESV2_PROGRAM_BINARY_H_
