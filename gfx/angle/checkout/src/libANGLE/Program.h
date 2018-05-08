//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.h: Defines the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#ifndef LIBANGLE_PROGRAM_H_
#define LIBANGLE_PROGRAM_H_

#include <GLES2/gl2.h>
#include <GLSLANG/ShaderVars.h>

#include <array>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "common/angleutils.h"
#include "common/mathutil.h"
#include "common/Optional.h"

#include "libANGLE/Constants.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/angletypes.h"

namespace rx
{
class GLImplFactory;
class ProgramImpl;
struct TranslatedAttribute;
}

namespace gl
{
struct Caps;
class Context;
class ContextState;
class Shader;
class ShaderProgramManager;
class State;
class InfoLog;
class Buffer;
class Framebuffer;

extern const char * const g_fakepath;

enum class LinkMismatchError
{
    // Shared
    NO_MISMATCH,
    TYPE_MISMATCH,
    ARRAY_SIZE_MISMATCH,
    PRECISION_MISMATCH,
    STRUCT_NAME_MISMATCH,
    FIELD_NUMBER_MISMATCH,
    FIELD_NAME_MISMATCH,

    // Varying specific
    INTERPOLATION_TYPE_MISMATCH,
    INVARIANCE_MISMATCH,

    // Uniform specific
    BINDING_MISMATCH,
    LOCATION_MISMATCH,
    OFFSET_MISMATCH,

    // Interface block specific
    LAYOUT_QUALIFIER_MISMATCH,
    MATRIX_PACKING_MISMATCH
};

class InfoLog : angle::NonCopyable
{
  public:
    InfoLog();
    ~InfoLog();

    size_t getLength() const;
    void getLog(GLsizei bufSize, GLsizei *length, char *infoLog) const;

    void appendSanitized(const char *message);
    void reset();

    // This helper class ensures we append a newline after writing a line.
    class StreamHelper : angle::NonCopyable
    {
      public:
        StreamHelper(StreamHelper &&rhs)
            : mStream(rhs.mStream)
        {
            rhs.mStream = nullptr;
        }

        StreamHelper &operator=(StreamHelper &&rhs)
        {
            std::swap(mStream, rhs.mStream);
            return *this;
        }

        ~StreamHelper()
        {
            // Write newline when destroyed on the stack
            if (mStream)
            {
                (*mStream) << std::endl;
            }
        }

        template <typename T>
        StreamHelper &operator<<(const T &value)
        {
            (*mStream) << value;
            return *this;
        }

      private:
        friend class InfoLog;

        StreamHelper(std::stringstream *stream)
            : mStream(stream)
        {
            ASSERT(stream);
        }

        std::stringstream *mStream;
    };

    template <typename T>
    StreamHelper operator<<(const T &value)
    {
        ensureInitialized();
        StreamHelper helper(mLazyStream.get());
        helper << value;
        return helper;
    }

    std::string str() const { return mLazyStream ? mLazyStream->str() : ""; }

    bool empty() const;

  private:
    void ensureInitialized()
    {
        if (!mLazyStream)
        {
            mLazyStream.reset(new std::stringstream());
        }
    }

    std::unique_ptr<std::stringstream> mLazyStream;
};

void LogLinkMismatch(InfoLog &infoLog,
                     const std::string &variableName,
                     const char *variableType,
                     LinkMismatchError linkError,
                     const std::string &mismatchedStructOrBlockFieldName,
                     ShaderType shaderType1,
                     ShaderType shaderType2);

bool IsActiveInterfaceBlock(const sh::InterfaceBlock &interfaceBlock);

// Struct used for correlating uniforms/elements of uniform arrays to handles
struct VariableLocation
{
    static constexpr unsigned int kUnused = GL_INVALID_INDEX;

    VariableLocation();
    VariableLocation(unsigned int arrayIndex, unsigned int index);

    // If used is false, it means this location is only used to fill an empty space in an array,
    // and there is no corresponding uniform variable for this location. It can also mean the
    // uniform was optimized out by the implementation.
    bool used() const { return (index != kUnused); }
    void markUnused() { index = kUnused; }
    void markIgnored() { ignored = true; }

    // "arrayIndex" stores the index of the innermost GLSL array. It's zero for non-arrays.
    unsigned int arrayIndex;
    // "index" is an index of the variable. The variable contains the indices for other than the
    // innermost GLSL arrays.
    unsigned int index;

    // If this location was bound to an unreferenced uniform.  Setting data on this uniform is a
    // no-op.
    bool ignored;
};

// Information about a variable binding.
// Currently used by CHROMIUM_path_rendering
struct BindingInfo
{
    // The type of binding, for example GL_FLOAT_VEC3.
    // This can be GL_NONE if the variable is optimized away.
    GLenum type;

    // This is the name of the variable in
    // the translated shader program. Note that
    // this can be empty in the case where the
    // variable has been optimized away.
    std::string name;

    // True if the binding is valid, otherwise false.
    bool valid;
};

// This small structure encapsulates binding sampler uniforms to active GL textures.
struct SamplerBinding
{
    SamplerBinding(TextureType textureTypeIn, size_t elementCount, bool unreferenced);
    SamplerBinding(const SamplerBinding &other);
    ~SamplerBinding();

    // Necessary for retrieving active textures from the GL state.
    TextureType textureType;

    // List of all textures bound to this sampler, of type textureType.
    std::vector<GLuint> boundTextureUnits;

    // A note if this sampler is an unreferenced uniform.
    bool unreferenced;
};

// A varying with tranform feedback enabled. If it's an array, either the whole array or one of its
// elements specified by 'arrayIndex' can set to be enabled.
struct TransformFeedbackVarying : public sh::Varying
{
    TransformFeedbackVarying(const sh::Varying &varyingIn, GLuint index)
        : sh::Varying(varyingIn), arrayIndex(index)
    {
        ASSERT(!isArrayOfArrays());
    }

    TransformFeedbackVarying(const sh::ShaderVariable &field, const sh::Varying &parent)
        : arrayIndex(GL_INVALID_INDEX)
    {
        sh::ShaderVariable *thisVar = this;
        *thisVar                    = field;
        interpolation               = parent.interpolation;
        isInvariant                 = parent.isInvariant;
        name                        = parent.name + "." + name;
    }

    std::string nameWithArrayIndex() const
    {
        std::stringstream fullNameStr;
        fullNameStr << name;
        if (arrayIndex != GL_INVALID_INDEX)
        {
            fullNameStr << "[" << arrayIndex << "]";
        }
        return fullNameStr.str();
    }
    GLsizei size() const
    {
        return (isArray() && arrayIndex == GL_INVALID_INDEX ? getOutermostArraySize() : 1);
    }

    GLuint arrayIndex;
};

struct ImageBinding
{
    ImageBinding(size_t count);
    ImageBinding(GLuint imageUnit, size_t count);
    ImageBinding(const ImageBinding &other);
    ~ImageBinding();

    std::vector<GLuint> boundImageUnits;
};

class ProgramState final : angle::NonCopyable
{
  public:
    ProgramState();
    ~ProgramState();

    const std::string &getLabel();

    Shader *getAttachedShader(ShaderType shaderType) const;
    const std::vector<std::string> &getTransformFeedbackVaryingNames() const
    {
        return mTransformFeedbackVaryingNames;
    }
    GLint getTransformFeedbackBufferMode() const { return mTransformFeedbackBufferMode; }
    GLuint getUniformBlockBinding(GLuint uniformBlockIndex) const
    {
        ASSERT(uniformBlockIndex < mUniformBlocks.size());
        return mUniformBlocks[uniformBlockIndex].binding;
    }
    GLuint getShaderStorageBlockBinding(GLuint blockIndex) const
    {
        ASSERT(blockIndex < mShaderStorageBlocks.size());
        return mShaderStorageBlocks[blockIndex].binding;
    }
    const UniformBlockBindingMask &getActiveUniformBlockBindingsMask() const
    {
        return mActiveUniformBlockBindings;
    }
    const std::vector<sh::Attribute> &getAttributes() const { return mAttributes; }
    const AttributesMask &getActiveAttribLocationsMask() const
    {
        return mActiveAttribLocationsMask;
    }
    unsigned int getMaxActiveAttribLocation() const { return mMaxActiveAttribLocation; }
    DrawBufferMask getActiveOutputVariables() const { return mActiveOutputVariables; }
    const std::vector<sh::OutputVariable> &getOutputVariables() const { return mOutputVariables; }
    const std::vector<VariableLocation> &getOutputLocations() const { return mOutputLocations; }
    const std::vector<LinkedUniform> &getUniforms() const { return mUniforms; }
    const std::vector<VariableLocation> &getUniformLocations() const { return mUniformLocations; }
    const std::vector<InterfaceBlock> &getUniformBlocks() const { return mUniformBlocks; }
    const std::vector<InterfaceBlock> &getShaderStorageBlocks() const
    {
        return mShaderStorageBlocks;
    }
    const std::vector<BufferVariable> &getBufferVariables() const { return mBufferVariables; }
    const std::vector<SamplerBinding> &getSamplerBindings() const { return mSamplerBindings; }
    const std::vector<ImageBinding> &getImageBindings() const { return mImageBindings; }
    const sh::WorkGroupSize &getComputeShaderLocalSize() const { return mComputeShaderLocalSize; }
    const RangeUI &getSamplerUniformRange() const { return mSamplerUniformRange; }
    const RangeUI &getImageUniformRange() const { return mImageUniformRange; }
    const RangeUI &getAtomicCounterUniformRange() const { return mAtomicCounterUniformRange; }

    const std::vector<TransformFeedbackVarying> &getLinkedTransformFeedbackVaryings() const
    {
        return mLinkedTransformFeedbackVaryings;
    }
    const std::vector<AtomicCounterBuffer> &getAtomicCounterBuffers() const
    {
        return mAtomicCounterBuffers;
    }

    GLuint getUniformIndexFromName(const std::string &name) const;
    GLuint getUniformIndexFromLocation(GLint location) const;
    Optional<GLuint> getSamplerIndex(GLint location) const;
    bool isSamplerUniformIndex(GLuint index) const;
    GLuint getSamplerIndexFromUniformIndex(GLuint uniformIndex) const;
    GLuint getAttributeLocation(const std::string &name) const;

    GLuint getBufferVariableIndexFromName(const std::string &name) const;

    int getNumViews() const { return mNumViews; }
    bool usesMultiview() const { return mNumViews != -1; }

    const ShaderBitSet &getLinkedShaderStages() const { return mLinkedShaderStages; }

  private:
    friend class MemoryProgramCache;
    friend class Program;

    void updateTransformFeedbackStrides();

    std::string mLabel;

    sh::WorkGroupSize mComputeShaderLocalSize;

    Shader *mAttachedFragmentShader;
    Shader *mAttachedVertexShader;
    Shader *mAttachedComputeShader;
    Shader *mAttachedGeometryShader;

    std::vector<std::string> mTransformFeedbackVaryingNames;
    std::vector<TransformFeedbackVarying> mLinkedTransformFeedbackVaryings;
    GLenum mTransformFeedbackBufferMode;

    // For faster iteration on the blocks currently being bound.
    UniformBlockBindingMask mActiveUniformBlockBindings;

    std::vector<sh::Attribute> mAttributes;
    angle::BitSet<MAX_VERTEX_ATTRIBS> mActiveAttribLocationsMask;
    unsigned int mMaxActiveAttribLocation;
    ComponentTypeMask mAttributesTypeMask;
    // mAttributesMask is identical to mActiveAttribLocationsMask with built-in attributes removed.
    AttributesMask mAttributesMask;

    // Uniforms are sorted in order:
    //  1. Non-opaque uniforms
    //  2. Sampler uniforms
    //  3. Image uniforms
    //  4. Atomic counter uniforms
    //  5. Uniform block uniforms
    // This makes opaque uniform validation easier, since we don't need a separate list.
    // For generating the entries and naming them we follow the spec: GLES 3.1 November 2016 section
    // 7.3.1.1 Naming Active Resources. There's a separate entry for each struct member and each
    // inner array of an array of arrays. Names and mapped names of uniforms that are arrays include
    // [0] in the end. This makes implementation of queries simpler.
    std::vector<LinkedUniform> mUniforms;

    std::vector<VariableLocation> mUniformLocations;
    std::vector<InterfaceBlock> mUniformBlocks;
    std::vector<BufferVariable> mBufferVariables;
    std::vector<InterfaceBlock> mShaderStorageBlocks;
    std::vector<AtomicCounterBuffer> mAtomicCounterBuffers;
    RangeUI mSamplerUniformRange;
    RangeUI mImageUniformRange;
    RangeUI mAtomicCounterUniformRange;

    // An array of the samplers that are used by the program
    std::vector<gl::SamplerBinding> mSamplerBindings;

    // An array of the images that are used by the program
    std::vector<gl::ImageBinding> mImageBindings;

    // Names and mapped names of output variables that are arrays include [0] in the end, similarly
    // to uniforms.
    std::vector<sh::OutputVariable> mOutputVariables;
    std::vector<VariableLocation> mOutputLocations;
    DrawBufferMask mActiveOutputVariables;

    // Fragment output variable base types: FLOAT, INT, or UINT.  Ordered by location.
    std::vector<GLenum> mOutputVariableTypes;
    ComponentTypeMask mDrawBufferTypeMask;

    bool mBinaryRetrieveableHint;
    bool mSeparable;
    ShaderBitSet mLinkedShaderStages;

    // ANGLE_multiview.
    int mNumViews;

    // GL_EXT_geometry_shader.
    GLenum mGeometryShaderInputPrimitiveType;
    GLenum mGeometryShaderOutputPrimitiveType;
    int mGeometryShaderInvocations;
    int mGeometryShaderMaxVertices;

    // The size of the data written to each transform feedback buffer per vertex.
    std::vector<GLsizei> mTransformFeedbackStrides;
};

class ProgramBindings final : angle::NonCopyable
{
  public:
    ProgramBindings();
    ~ProgramBindings();

    void bindLocation(GLuint index, const std::string &name);
    int getBinding(const std::string &name) const;

    using const_iterator = std::unordered_map<std::string, GLuint>::const_iterator;
    const_iterator begin() const;
    const_iterator end() const;

  private:
    std::unordered_map<std::string, GLuint> mBindings;
};

struct ProgramVaryingRef
{
    const sh::Varying *get() const { return vertex ? vertex : fragment; }

    const sh::Varying *vertex   = nullptr;
    const sh::Varying *fragment = nullptr;
};

using ProgramMergedVaryings = std::map<std::string, ProgramVaryingRef>;

class Program final : angle::NonCopyable, public LabeledObject
{
  public:
    Program(rx::GLImplFactory *factory, ShaderProgramManager *manager, GLuint handle);
    void onDestroy(const Context *context);

    GLuint id() const { return mHandle; }

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    rx::ProgramImpl *getImplementation() const { return mProgram; }

    void attachShader(Shader *shader);
    void detachShader(const Context *context, Shader *shader);
    int getAttachedShadersCount() const;

    const Shader *getAttachedShader(ShaderType shaderType) const;

    void bindAttributeLocation(GLuint index, const char *name);
    void bindUniformLocation(GLuint index, const char *name);

    // CHROMIUM_path_rendering
    BindingInfo getFragmentInputBindingInfo(const Context *context, GLint index) const;
    void bindFragmentInputLocation(GLint index, const char *name);
    void pathFragmentInputGen(const Context *context,
                              GLint index,
                              GLenum genMode,
                              GLint components,
                              const GLfloat *coeffs);

    Error link(const gl::Context *context);
    bool isLinked() const;

    bool hasLinkedShaderStage(ShaderType shaderType) const;

    Error loadBinary(const Context *context,
                     GLenum binaryFormat,
                     const void *binary,
                     GLsizei length);
    Error saveBinary(const Context *context,
                     GLenum *binaryFormat,
                     void *binary,
                     GLsizei bufSize,
                     GLsizei *length) const;
    GLint getBinaryLength(const Context *context) const;
    void setBinaryRetrievableHint(bool retrievable);
    bool getBinaryRetrievableHint() const;

    void setSeparable(bool separable);
    bool isSeparable() const;

    int getInfoLogLength() const;
    void getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const;
    void getAttachedShaders(GLsizei maxCount, GLsizei *count, GLuint *shaders) const;

    GLuint getAttributeLocation(const std::string &name) const;
    bool isAttribLocationActive(size_t attribLocation) const;

    void getActiveAttribute(GLuint index,
                            GLsizei bufsize,
                            GLsizei *length,
                            GLint *size,
                            GLenum *type,
                            GLchar *name) const;
    GLint getActiveAttributeCount() const;
    GLint getActiveAttributeMaxLength() const;
    const std::vector<sh::Attribute> &getAttributes() const { return mState.mAttributes; }

    GLint getFragDataLocation(const std::string &name) const;
    size_t getOutputResourceCount() const;
    const std::vector<GLenum> &getOutputVariableTypes() const
    {
        return mState.mOutputVariableTypes;
    }
    DrawBufferMask getActiveOutputVariables() const { return mState.mActiveOutputVariables; }

    void getActiveUniform(GLuint index,
                          GLsizei bufsize,
                          GLsizei *length,
                          GLint *size,
                          GLenum *type,
                          GLchar *name) const;
    GLint getActiveUniformCount() const;
    size_t getActiveBufferVariableCount() const;
    GLint getActiveUniformMaxLength() const;
    bool isValidUniformLocation(GLint location) const;
    const LinkedUniform &getUniformByLocation(GLint location) const;
    const VariableLocation &getUniformLocation(GLint location) const;
    const std::vector<VariableLocation> &getUniformLocations() const;
    const LinkedUniform &getUniformByIndex(GLuint index) const;

    const BufferVariable &getBufferVariableByIndex(GLuint index) const;

    enum SetUniformResult
    {
        SamplerChanged,
        NoSamplerChange,
    };

    GLint getUniformLocation(const std::string &name) const;
    GLuint getUniformIndex(const std::string &name) const;
    void setUniform1fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform2fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform3fv(GLint location, GLsizei count, const GLfloat *v);
    void setUniform4fv(GLint location, GLsizei count, const GLfloat *v);
    SetUniformResult setUniform1iv(GLint location, GLsizei count, const GLint *v);
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

    void getUniformfv(const Context *context, GLint location, GLfloat *params) const;
    void getUniformiv(const Context *context, GLint location, GLint *params) const;
    void getUniformuiv(const Context *context, GLint location, GLuint *params) const;

    void getActiveUniformBlockName(const GLuint blockIndex,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLchar *blockName) const;
    void getActiveShaderStorageBlockName(const GLuint blockIndex,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLchar *blockName) const;
    GLuint getActiveUniformBlockCount() const;
    GLuint getActiveAtomicCounterBufferCount() const;
    GLuint getActiveShaderStorageBlockCount() const;
    GLint getActiveUniformBlockMaxNameLength() const;
    GLint getActiveShaderStorageBlockMaxNameLength() const;

    GLuint getUniformBlockIndex(const std::string &name) const;
    GLuint getShaderStorageBlockIndex(const std::string &name) const;

    void bindUniformBlock(GLuint uniformBlockIndex, GLuint uniformBlockBinding);
    GLuint getUniformBlockBinding(GLuint uniformBlockIndex) const;
    GLuint getShaderStorageBlockBinding(GLuint shaderStorageBlockIndex) const;

    const InterfaceBlock &getUniformBlockByIndex(GLuint index) const;
    const InterfaceBlock &getShaderStorageBlockByIndex(GLuint index) const;

    void setTransformFeedbackVaryings(GLsizei count, const GLchar *const *varyings, GLenum bufferMode);
    void getTransformFeedbackVarying(GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name) const;
    GLsizei getTransformFeedbackVaryingCount() const;
    GLsizei getTransformFeedbackVaryingMaxLength() const;
    GLenum getTransformFeedbackBufferMode() const;
    GLuint getTransformFeedbackVaryingResourceIndex(const GLchar *name) const;
    const TransformFeedbackVarying &getTransformFeedbackVaryingResource(GLuint index) const;

    void addRef();
    void release(const Context *context);
    unsigned int getRefCount() const;
    void flagForDeletion();
    bool isFlaggedForDeletion() const;

    void validate(const Caps &caps);
    bool validateSamplers(InfoLog *infoLog, const Caps &caps);
    bool isValidated() const;
    bool samplesFromTexture(const gl::State &state, GLuint textureID) const;

    const AttributesMask &getActiveAttribLocationsMask() const
    {
        return mState.mActiveAttribLocationsMask;
    }

    const std::vector<SamplerBinding> &getSamplerBindings() const
    {
        return mState.mSamplerBindings;
    }

    const std::vector<ImageBinding> &getImageBindings() const { return mState.mImageBindings; }
    const sh::WorkGroupSize &getComputeShaderLocalSize() const
    {
        return mState.mComputeShaderLocalSize;
    }

    GLenum getGeometryShaderInputPrimitiveType() const
    {
        return mState.mGeometryShaderInputPrimitiveType;
    }
    GLenum getGeometryShaderOutputPrimitiveType() const
    {
        return mState.mGeometryShaderOutputPrimitiveType;
    }
    GLint getGeometryShaderInvocations() const { return mState.mGeometryShaderInvocations; }
    GLint getGeometryShaderMaxVertices() const { return mState.mGeometryShaderMaxVertices; }

    const ProgramState &getState() const { return mState; }

    static LinkMismatchError LinkValidateVariablesBase(
        const sh::ShaderVariable &variable1,
        const sh::ShaderVariable &variable2,
        bool validatePrecision,
        bool validateArraySize,
        std::string *mismatchedStructOrBlockMemberName);

    GLuint getInputResourceIndex(const GLchar *name) const;
    GLuint getOutputResourceIndex(const GLchar *name) const;
    void getInputResourceName(GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) const;
    void getOutputResourceName(GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) const;
    void getUniformResourceName(GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) const;
    void getBufferVariableResourceName(GLuint index,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLchar *name) const;
    const sh::Attribute &getInputResource(GLuint index) const;
    const sh::OutputVariable &getOutputResource(GLuint index) const;

    const ProgramBindings &getAttributeBindings() const { return mAttributeBindings; }
    const ProgramBindings &getUniformLocationBindings() const { return mUniformLocationBindings; }
    const ProgramBindings &getFragmentInputBindings() const { return mFragmentInputBindings; }

    int getNumViews() const { return mState.getNumViews(); }
    bool usesMultiview() const { return mState.usesMultiview(); }

    ComponentTypeMask getDrawBufferTypeMask() const { return mState.mDrawBufferTypeMask; }
    ComponentTypeMask getAttributesTypeMask() const { return mState.mAttributesTypeMask; }
    AttributesMask getAttributesMask() const { return mState.mAttributesMask; }

    const std::vector<GLsizei> &getTransformFeedbackStrides() const
    {
        return mState.mTransformFeedbackStrides;
    }

  private:
    ~Program() override;

    void unlink();

    bool linkValidateShaders(const Context *context, InfoLog &infoLog);
    bool linkAttributes(const Context *context, InfoLog &infoLog);
    bool linkInterfaceBlocks(const Context *context, InfoLog &infoLog);
    bool linkVaryings(const Context *context, InfoLog &infoLog) const;

    bool linkUniforms(const Context *context,
                      InfoLog &infoLog,
                      const ProgramBindings &uniformLocationBindings);
    void linkSamplerAndImageBindings();
    bool linkAtomicCounterBuffers();

    void updateLinkedShaderStages();

    static LinkMismatchError LinkValidateVaryings(const sh::Varying &outputVarying,
                                                  const sh::Varying &inputVarying,
                                                  int shaderVersion,
                                                  bool validateGeometryShaderInputVarying,
                                                  std::string *mismatchedStructFieldName);

    bool linkValidateShaderInterfaceMatching(const Context *context,
                                             Shader *generatingShader,
                                             Shader *consumingShader,
                                             InfoLog &infoLog) const;

    // Check for aliased path rendering input bindings (if any).
    // If more than one binding refer statically to the same location the link must fail.
    bool linkValidateFragmentInputBindings(const Context *context, InfoLog &infoLog) const;

    bool linkValidateBuiltInVaryings(const Context *context, InfoLog &infoLog) const;
    bool linkValidateTransformFeedback(const gl::Context *context,
                                       InfoLog &infoLog,
                                       const ProgramMergedVaryings &linkedVaryings,
                                       const Caps &caps) const;
    bool linkValidateGlobalNames(const Context *context, InfoLog &infoLog) const;

    void gatherTransformFeedbackVaryings(const ProgramMergedVaryings &varyings);

    ProgramMergedVaryings getMergedVaryings(const Context *context) const;
    void linkOutputVariables(const Context *context);

    void setUniformValuesFromBindingQualifiers();

    void initInterfaceBlockBindings();

    // Both these function update the cached uniform values and return a modified "count"
    // so that the uniform update doesn't overflow the uniform.
    template <typename T>
    GLsizei clampUniformCount(const VariableLocation &locationInfo,
                              GLsizei count,
                              int vectorSize,
                              const T *v);
    template <size_t cols, size_t rows, typename T>
    GLsizei clampMatrixUniformCount(GLint location, GLsizei count, GLboolean transpose, const T *v);

    void updateSamplerUniform(const VariableLocation &locationInfo,
                              GLsizei clampedCount,
                              const GLint *v);

    template <typename DestT>
    void getUniformInternal(const Context *context,
                            DestT *dataOut,
                            GLint location,
                            GLenum nativeType,
                            int components) const;

    template <typename T>
    void getResourceName(GLuint index,
                         const std::vector<T> &resources,
                         GLsizei bufSize,
                         GLsizei *length,
                         GLchar *name) const;

    template <typename T>
    GLint getActiveInterfaceBlockMaxNameLength(const std::vector<T> &resources) const;

    ProgramState mState;
    rx::ProgramImpl *mProgram;

    bool mValidated;

    ProgramBindings mAttributeBindings;

    // Note that this has nothing to do with binding layout qualifiers that can be set for some
    // uniforms in GLES3.1+. It is used to pre-set the location of uniforms.
    ProgramBindings mUniformLocationBindings;

    // CHROMIUM_path_rendering
    ProgramBindings mFragmentInputBindings;

    bool mLinked;
    bool mDeleteStatus;   // Flag to indicate that the program can be deleted when no longer in use

    unsigned int mRefCount;

    ShaderProgramManager *mResourceManager;
    const GLuint mHandle;

    InfoLog mInfoLog;

    // Cache for sampler validation
    Optional<bool> mCachedValidateSamplersResult;
    std::vector<TextureType> mTextureUnitTypesCache;
};
}  // namespace gl

#endif   // LIBANGLE_PROGRAM_H_
