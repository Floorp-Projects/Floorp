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
struct PackedVarying;

extern const char * const g_fakepath;

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

// Struct used for correlating uniforms/elements of uniform arrays to handles
struct VariableLocation
{
    static constexpr unsigned int kUnused = GL_INVALID_INDEX;

    VariableLocation();
    VariableLocation(unsigned int element, unsigned int index);

    // If used is false, it means this location is only used to fill an empty space in an array,
    // and there is no corresponding uniform variable for this location. It can also mean the
    // uniform was optimized out by the implementation.
    bool used() const { return (index != kUnused); }
    void markUnused() { index = kUnused; }
    void markIgnored() { ignored = true; }

    unsigned int element;
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
    SamplerBinding(GLenum textureTypeIn, size_t elementCount, bool unreferenced)
        : textureType(textureTypeIn), boundTextureUnits(elementCount, 0), unreferenced(unreferenced)
    {
    }

    // Necessary for retrieving active textures from the GL state.
    GLenum textureType;

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
    GLsizei size() const { return (arrayIndex == GL_INVALID_INDEX ? elementCount() : 1); }

    GLuint arrayIndex;
};

struct ImageBinding
{
    ImageBinding(size_t count) : boundImageUnits(count, 0) {}
    ImageBinding(GLuint imageUnit, size_t count)
    {
        for (size_t index = 0; index < count; ++index)
        {
            boundImageUnits.push_back(imageUnit + static_cast<GLuint>(index));
        }
    }

    std::vector<GLuint> boundImageUnits;
};

class ProgramState final : angle::NonCopyable
{
  public:
    ProgramState();
    ~ProgramState();

    const std::string &getLabel();

    Shader *getAttachedVertexShader() const { return mAttachedVertexShader; }
    Shader *getAttachedFragmentShader() const { return mAttachedFragmentShader; }
    Shader *getAttachedComputeShader() const { return mAttachedComputeShader; }
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
    DrawBufferMask getActiveOutputVariables() const { return mActiveOutputVariables; }
    const std::vector<sh::OutputVariable> &getOutputVariables() const { return mOutputVariables; }
    const std::map<int, VariableLocation> &getOutputLocations() const { return mOutputLocations; }
    const std::vector<LinkedUniform> &getUniforms() const { return mUniforms; }
    const std::vector<VariableLocation> &getUniformLocations() const { return mUniformLocations; }
    const std::vector<InterfaceBlock> &getUniformBlocks() const { return mUniformBlocks; }
    const std::vector<InterfaceBlock> &getShaderStorageBlocks() const
    {
        return mShaderStorageBlocks;
    }
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

    GLint getUniformLocation(const std::string &name) const;
    GLuint getUniformIndexFromName(const std::string &name) const;
    GLuint getUniformIndexFromLocation(GLint location) const;
    Optional<GLuint> getSamplerIndex(GLint location) const;
    bool isSamplerUniformIndex(GLuint index) const;
    GLuint getSamplerIndexFromUniformIndex(GLuint uniformIndex) const;
    GLuint getAttributeLocation(const std::string &name) const;

    int getNumViews() const { return mNumViews; }
    bool usesMultiview() const { return mNumViews != -1; }

  private:
    friend class MemoryProgramCache;
    friend class Program;

    std::string mLabel;

    sh::WorkGroupSize mComputeShaderLocalSize;

    Shader *mAttachedFragmentShader;
    Shader *mAttachedVertexShader;
    Shader *mAttachedComputeShader;

    std::vector<std::string> mTransformFeedbackVaryingNames;
    std::vector<TransformFeedbackVarying> mLinkedTransformFeedbackVaryings;
    GLenum mTransformFeedbackBufferMode;

    // For faster iteration on the blocks currently being bound.
    UniformBlockBindingMask mActiveUniformBlockBindings;

    std::vector<sh::Attribute> mAttributes;
    angle::BitSet<MAX_VERTEX_ATTRIBS> mActiveAttribLocationsMask;

    // Uniforms are sorted in order:
    //  1. Non-opaque uniforms
    //  2. Sampler uniforms
    //  3. Image uniforms
    //  4. Atomic counter uniforms
    //  5. Uniform block uniforms
    // This makes opaque uniform validation easier, since we don't need a separate list.
    std::vector<LinkedUniform> mUniforms;
    std::vector<VariableLocation> mUniformLocations;
    std::vector<InterfaceBlock> mUniformBlocks;
    std::vector<InterfaceBlock> mShaderStorageBlocks;
    std::vector<AtomicCounterBuffer> mAtomicCounterBuffers;
    RangeUI mSamplerUniformRange;
    RangeUI mImageUniformRange;
    RangeUI mAtomicCounterUniformRange;

    // An array of the samplers that are used by the program
    std::vector<gl::SamplerBinding> mSamplerBindings;

    // An array of the images that are used by the program
    std::vector<gl::ImageBinding> mImageBindings;

    std::vector<sh::OutputVariable> mOutputVariables;
    std::map<int, VariableLocation> mOutputLocations;
    DrawBufferMask mActiveOutputVariables;

    // Fragment output variable base types: FLOAT, INT, or UINT.  Ordered by location.
    std::vector<GLenum> mOutputVariableTypes;

    bool mBinaryRetrieveableHint;
    bool mSeparable;

    // ANGLE_multiview.
    int mNumViews;
};

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

    const Shader *getAttachedVertexShader() const { return mState.mAttachedVertexShader; }
    const Shader *getAttachedFragmentShader() const { return mState.mAttachedFragmentShader; }
    const Shader *getAttachedComputeShader() const { return mState.mAttachedComputeShader; }

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
    GLint getActiveUniformMaxLength() const;
    GLint getActiveUniformi(GLuint index, GLenum pname) const;
    bool isValidUniformLocation(GLint location) const;
    const LinkedUniform &getUniformByLocation(GLint location) const;
    const VariableLocation &getUniformLocation(GLint location) const;
    const std::vector<VariableLocation> &getUniformLocations() const;
    const LinkedUniform &getUniformByIndex(GLuint index) const;

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

    void getActiveUniformBlockName(GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName) const;
    GLuint getActiveUniformBlockCount() const;
    GLuint getActiveShaderStorageBlockCount() const;
    GLint getActiveUniformBlockMaxLength() const;

    GLuint getUniformBlockIndex(const std::string &name) const;

    void bindUniformBlock(GLuint uniformBlockIndex, GLuint uniformBlockBinding);
    GLuint getUniformBlockBinding(GLuint uniformBlockIndex) const;
    GLuint getShaderStorageBlockBinding(GLuint shaderStorageBlockIndex) const;

    const InterfaceBlock &getUniformBlockByIndex(GLuint index) const;

    void setTransformFeedbackVaryings(GLsizei count, const GLchar *const *varyings, GLenum bufferMode);
    void getTransformFeedbackVarying(GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name) const;
    GLsizei getTransformFeedbackVaryingCount() const;
    GLsizei getTransformFeedbackVaryingMaxLength() const;
    GLenum getTransformFeedbackBufferMode() const;

    static bool linkValidateInterfaceBlockFields(InfoLog &infoLog,
                                                 const std::string &uniformName,
                                                 const sh::InterfaceBlockField &vertexUniform,
                                                 const sh::InterfaceBlockField &fragmentUniform,
                                                 bool webglCompatibility);

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

    const ProgramState &getState() const { return mState; }

    static bool linkValidateVariablesBase(InfoLog &infoLog,
                                          const std::string &variableName,
                                          const sh::ShaderVariable &vertexVariable,
                                          const sh::ShaderVariable &fragmentVariable,
                                          bool validatePrecision);

    GLuint getInputResourceIndex(const GLchar *name) const;
    GLuint getOutputResourceIndex(const GLchar *name) const;
    void getInputResourceName(GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) const;
    void getOutputResourceName(GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) const;
    const sh::Attribute &getInputResource(GLuint index) const;
    const sh::OutputVariable &getOutputResource(GLuint index) const;

    class Bindings final : angle::NonCopyable
    {
      public:
        void bindLocation(GLuint index, const std::string &name);
        int getBinding(const std::string &name) const;

        typedef std::unordered_map<std::string, GLuint>::const_iterator const_iterator;
        const_iterator begin() const;
        const_iterator end() const;

      private:
        std::unordered_map<std::string, GLuint> mBindings;
    };

    const Bindings &getAttributeBindings() const { return mAttributeBindings; }
    const Bindings &getUniformLocationBindings() const { return mUniformLocationBindings; }
    const Bindings &getFragmentInputBindings() const { return mFragmentInputBindings; }

    int getNumViews() const { return mState.getNumViews(); }
    bool usesMultiview() const { return mState.usesMultiview(); }

  private:
    ~Program();

    struct VaryingRef
    {
        const sh::Varying *get() const { return vertex ? vertex : fragment; }

        const sh::Varying *vertex   = nullptr;
        const sh::Varying *fragment = nullptr;
    };

    using MergedVaryings = std::map<std::string, VaryingRef>;

    void unlink();

    bool linkAttributes(const Context *context, InfoLog &infoLog);
    bool validateVertexAndFragmentInterfaceBlocks(
        const std::vector<sh::InterfaceBlock> &vertexInterfaceBlocks,
        const std::vector<sh::InterfaceBlock> &fragmentInterfaceBlocks,
        InfoLog &infoLog,
        bool webglCompatibility) const;
    bool linkInterfaceBlocks(const Context *context, InfoLog &infoLog);
    bool linkVaryings(const Context *context, InfoLog &infoLog) const;

    bool linkUniforms(const Context *context,
                      InfoLog &infoLog,
                      const Bindings &uniformLocationBindings);
    void linkSamplerAndImageBindings();
    bool linkAtomicCounterBuffers();

    bool areMatchingInterfaceBlocks(InfoLog &infoLog,
                                    const sh::InterfaceBlock &vertexInterfaceBlock,
                                    const sh::InterfaceBlock &fragmentInterfaceBlock,
                                    bool webglCompatibility) const;

    static bool linkValidateVaryings(InfoLog &infoLog,
                                     const std::string &varyingName,
                                     const sh::Varying &vertexVarying,
                                     const sh::Varying &fragmentVarying,
                                     int shaderVersion);
    bool linkValidateBuiltInVaryings(const Context *context, InfoLog &infoLog) const;
    bool linkValidateTransformFeedback(const gl::Context *context,
                                       InfoLog &infoLog,
                                       const MergedVaryings &linkedVaryings,
                                       const Caps &caps) const;
    bool linkValidateGlobalNames(const Context *context, InfoLog &infoLog) const;

    void gatherTransformFeedbackVaryings(const MergedVaryings &varyings);

    MergedVaryings getMergedVaryings(const Context *context) const;
    std::vector<PackedVarying> getPackedVaryings(const MergedVaryings &mergedVaryings) const;
    void linkOutputVariables(const Context *context);

    void setUniformValuesFromBindingQualifiers();

    void gatherAtomicCounterBuffers();
    void gatherComputeBlockInfo(const std::vector<sh::InterfaceBlock> &computeBlocks);
    void gatherVertexAndFragmentBlockInfo(
        const std::vector<sh::InterfaceBlock> &vertexInterfaceBlocks,
        const std::vector<sh::InterfaceBlock> &fragmentInterfaceBlocks);
    void gatherInterfaceBlockInfo(const Context *context);
    template <typename VarT>
    void defineUniformBlockMembers(const std::vector<VarT> &fields,
                                   const std::string &prefix,
                                   const std::string &mappedPrefix,
                                   int blockIndex);

    void defineInterfaceBlock(const sh::InterfaceBlock &interfaceBlock, GLenum shaderType);

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

    ProgramState mState;
    rx::ProgramImpl *mProgram;

    bool mValidated;

    Bindings mAttributeBindings;

    // Note that this has nothing to do with binding layout qualifiers that can be set for some
    // uniforms in GLES3.1+. It is used to pre-set the location of uniforms.
    Bindings mUniformLocationBindings;

    // CHROMIUM_path_rendering
    Bindings mFragmentInputBindings;

    bool mLinked;
    bool mDeleteStatus;   // Flag to indicate that the program can be deleted when no longer in use

    unsigned int mRefCount;

    ShaderProgramManager *mResourceManager;
    const GLuint mHandle;

    InfoLog mInfoLog;

    // Cache for sampler validation
    Optional<bool> mCachedValidateSamplersResult;
    std::vector<GLenum> mTextureUnitTypesCache;
};
}  // namespace gl

#endif   // LIBANGLE_PROGRAM_H_
