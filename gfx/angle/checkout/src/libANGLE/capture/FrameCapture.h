
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FrameCapture.h:
//   ANGLE Frame capture inteface.
//

#ifndef LIBANGLE_FRAME_CAPTURE_H_
#define LIBANGLE_FRAME_CAPTURE_H_

#include "common/PackedEnums.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/capture/frame_capture_utils_autogen.h"
#include "libANGLE/entry_points_utils.h"

namespace gl
{
enum class GLenumGroup;
}

namespace angle
{

using ParamData = std::vector<std::vector<uint8_t>>;
struct ParamCapture : angle::NonCopyable
{
    ParamCapture();
    ParamCapture(const char *nameIn, ParamType typeIn);
    ~ParamCapture();

    ParamCapture(ParamCapture &&other);
    ParamCapture &operator=(ParamCapture &&other);

    std::string name;
    ParamType type;
    ParamValue value;
    gl::GLenumGroup enumGroup;  // only used for param type GLenum, GLboolean and GLbitfield
    ParamData data;
    int arrayClientPointerIndex = -1;
    size_t readBufferSizeBytes  = 0;
};

class ParamBuffer final : angle::NonCopyable
{
  public:
    ParamBuffer();
    ~ParamBuffer();

    ParamBuffer(ParamBuffer &&other);
    ParamBuffer &operator=(ParamBuffer &&other);

    template <typename T>
    void addValueParam(const char *paramName, ParamType paramType, T paramValue);
    template <typename T>
    void addEnumParam(const char *paramName,
                      gl::GLenumGroup enumGroup,
                      ParamType paramType,
                      T paramValue);

    ParamCapture &getParam(const char *paramName, ParamType paramType, int index);
    const ParamCapture &getParam(const char *paramName, ParamType paramType, int index) const;
    ParamCapture &getParamFlexName(const char *paramName1,
                                   const char *paramName2,
                                   ParamType paramType,
                                   int index);
    const ParamCapture &getParamFlexName(const char *paramName1,
                                         const char *paramName2,
                                         ParamType paramType,
                                         int index) const;
    const ParamCapture &getReturnValue() const { return mReturnValueCapture; }

    void addParam(ParamCapture &&param);
    void addReturnValue(ParamCapture &&returnValue);
    bool hasClientArrayData() const { return mClientArrayDataParam != -1; }
    ParamCapture &getClientArrayPointerParameter();
    size_t getReadBufferSize() const { return mReadBufferSize; }

    const std::vector<ParamCapture> &getParamCaptures() const { return mParamCaptures; }

    // These helpers allow us to track the ID of the buffer that was active when
    // MapBufferRange was called.  We'll use it during replay to track the
    // buffer's contents, as they can be modified by the host.
    void setMappedBufferID(gl::BufferID bufferID) { mMappedBufferID = bufferID; }
    gl::BufferID getMappedBufferID() const { return mMappedBufferID; }

  private:
    std::vector<ParamCapture> mParamCaptures;
    ParamCapture mReturnValueCapture;
    int mClientArrayDataParam = -1;
    size_t mReadBufferSize    = 0;
    gl::BufferID mMappedBufferID;
};

struct CallCapture
{
    CallCapture(EntryPoint entryPointIn, ParamBuffer &&paramsIn);
    CallCapture(const std::string &customFunctionNameIn, ParamBuffer &&paramsIn);
    ~CallCapture();

    CallCapture(CallCapture &&other);
    CallCapture &operator=(CallCapture &&other);

    const char *name() const;

    EntryPoint entryPoint;
    std::string customFunctionName;
    ParamBuffer params;
};

class ReplayContext
{
  public:
    ReplayContext(size_t readBufferSizebytes, const gl::AttribArray<size_t> &clientArraysSizebytes);
    ~ReplayContext();

    template <typename T>
    T getReadBufferPointer(const ParamCapture &param)
    {
        ASSERT(param.readBufferSizeBytes > 0);
        ASSERT(mReadBuffer.size() >= param.readBufferSizeBytes);
        return reinterpret_cast<T>(mReadBuffer.data());
    }
    template <typename T>
    T getAsConstPointer(const ParamCapture &param)
    {
        if (param.arrayClientPointerIndex != -1)
        {
            return reinterpret_cast<T>(mClientArraysBuffer[param.arrayClientPointerIndex].data());
        }

        if (!param.data.empty())
        {
            ASSERT(param.data.size() == 1);
            return reinterpret_cast<T>(param.data[0].data());
        }

        return nullptr;
    }

    template <typename T>
    T getAsPointerConstPointer(const ParamCapture &param)
    {
        static_assert(sizeof(typename std::remove_pointer<T>::type) == sizeof(uint8_t *),
                      "pointer size not match!");

        ASSERT(!param.data.empty());
        mPointersBuffer.clear();
        mPointersBuffer.reserve(param.data.size());
        for (const std::vector<uint8_t> &data : param.data)
        {
            mPointersBuffer.emplace_back(data.data());
        }
        return reinterpret_cast<T>(mPointersBuffer.data());
    }

    gl::AttribArray<std::vector<uint8_t>> &getClientArraysBuffer() { return mClientArraysBuffer; }

  private:
    std::vector<uint8_t> mReadBuffer;
    std::vector<const uint8_t *> mPointersBuffer;
    gl::AttribArray<std::vector<uint8_t>> mClientArraysBuffer;
};

// Helper to use unique IDs for each local data variable.
class DataCounters final : angle::NonCopyable
{
  public:
    DataCounters();
    ~DataCounters();

    int getAndIncrement(EntryPoint entryPoint, const std::string &paramName);

  private:
    // <CallName, ParamName>
    using Counter = std::pair<EntryPoint, std::string>;
    std::map<Counter, int> mData;
};

constexpr int kStringsNotFound = -1;
class StringCounters final : angle::NonCopyable
{
  public:
    StringCounters();
    ~StringCounters();

    int getStringCounter(std::vector<std::string> &str);
    void setStringCounter(std::vector<std::string> &str, int &counter);

  private:
    std::map<std::vector<std::string>, int> mStringCounterMap;
};

class DataTracker final : angle::NonCopyable
{
  public:
    DataTracker();
    ~DataTracker();

    DataCounters &getCounters() { return mCounters; }
    StringCounters &getStringCounters() { return mStringCounters; }

  private:
    DataCounters mCounters;
    StringCounters mStringCounters;
};

using BufferSet   = std::set<gl::BufferID>;
using BufferCalls = std::map<gl::BufferID, std::vector<CallCapture>>;

// true means mapped, false means unmapped
using BufferMapStatusMap = std::map<gl::BufferID, bool>;

using FenceSyncSet   = std::set<GLsync>;
using FenceSyncCalls = std::map<GLsync, std::vector<CallCapture>>;

// Helper to track resource changes during the capture
class ResourceTracker final : angle::NonCopyable
{
  public:
    ResourceTracker();
    ~ResourceTracker();

    BufferCalls &getBufferRegenCalls() { return mBufferRegenCalls; }
    BufferCalls &getBufferRestoreCalls() { return mBufferRestoreCalls; }
    BufferCalls &getBufferMapCalls() { return mBufferMapCalls; }
    BufferCalls &getBufferUnmapCalls() { return mBufferUnmapCalls; }

    std::vector<CallCapture> &getBufferBindingCalls() { return mBufferBindingCalls; }

    BufferSet &getStartingBuffers() { return mStartingBuffers; }
    BufferSet &getNewBuffers() { return mNewBuffers; }
    BufferSet &getBuffersToRegen() { return mBuffersToRegen; }
    BufferSet &getBuffersToRestore() { return mBuffersToRestore; }

    void setGennedBuffer(gl::BufferID id);
    void setDeletedBuffer(gl::BufferID id);
    void setBufferModified(gl::BufferID id);
    void setBufferMapped(gl::BufferID id);
    void setBufferUnmapped(gl::BufferID id);

    const bool &getStartingBuffersMappedCurrent(gl::BufferID id)
    {
        return mStartingBuffersMappedCurrent[id];
    }
    const bool &getStartingBuffersMappedInitial(gl::BufferID id)
    {
        return mStartingBuffersMappedInitial[id];
    }
    void setStartingBufferMapped(gl::BufferID id, bool mapped)
    {
        // Track the current state (which will change throughout the trace)
        mStartingBuffersMappedCurrent[id] = mapped;

        // And the initial state, to compare during frame loop reset
        mStartingBuffersMappedInitial[id] = mapped;
    }

    void onShaderProgramAccess(gl::ShaderProgramID shaderProgramID);
    uint32_t getMaxShaderPrograms() const { return mMaxShaderPrograms; }

    FenceSyncSet &getStartingFenceSyncs() { return mStartingFenceSyncs; }
    FenceSyncCalls &getFenceSyncRegenCalls() { return mFenceSyncRegenCalls; }
    FenceSyncSet &getFenceSyncsToRegen() { return mFenceSyncsToRegen; }
    void setDeletedFenceSync(GLsync sync);

  private:
    // Buffer regen calls will delete and gen a buffer
    BufferCalls mBufferRegenCalls;
    // Buffer restore calls will restore the contents of a buffer
    BufferCalls mBufferRestoreCalls;
    // Buffer map calls will map a buffer with correct offset, length, and access flags
    BufferCalls mBufferMapCalls;
    // Buffer unmap calls will bind and unmap a given buffer
    BufferCalls mBufferUnmapCalls;

    // Buffer binding calls to restore bindings recorded during MEC
    std::vector<CallCapture> mBufferBindingCalls;

    // Starting buffers include all the buffers created during setup for MEC
    BufferSet mStartingBuffers;
    // New buffers are those generated while capturing
    BufferSet mNewBuffers;
    // Buffers to regen are a list of starting buffers that need to be deleted and genned
    BufferSet mBuffersToRegen;
    // Buffers to restore include any starting buffers with contents modified during the run
    BufferSet mBuffersToRestore;

    // Whether a given buffer was mapped at the start of the trace
    BufferMapStatusMap mStartingBuffersMappedInitial;
    // The status of buffer mapping throughout the trace, modified with each Map/Unmap call
    BufferMapStatusMap mStartingBuffersMappedCurrent;

    // Maximum accessed shader program ID.
    uint32_t mMaxShaderPrograms = 0;

    // Fence sync objects created during MEC setup
    FenceSyncSet mStartingFenceSyncs;
    // Fence sync regen calls will create a fence sync objects
    FenceSyncCalls mFenceSyncRegenCalls;
    // Fence syncs to regen are a list of starting fence sync objects that were deleted and need to
    // be regen'ed.
    FenceSyncSet mFenceSyncsToRegen;
};

// Used by the CPP replay to filter out unnecessary code.
using HasResourceTypeMap = angle::PackedEnumBitSet<ResourceIDType>;

// Map of buffer ID to offset and size used when mapped
using BufferDataMap = std::map<gl::BufferID, std::pair<GLintptr, GLsizeiptr>>;

// A dictionary of sources indexed by shader type.
using ProgramSources = gl::ShaderMap<std::string>;

// Maps from IDs to sources.
using ShaderSourceMap  = std::map<gl::ShaderProgramID, std::string>;
using ProgramSourceMap = std::map<gl::ShaderProgramID, ProgramSources>;

// Map from textureID to level and data
using TextureLevels       = std::map<GLint, std::vector<uint8_t>>;
using TextureLevelDataMap = std::map<gl::TextureID, TextureLevels>;

// Map from ContextID to surface dimensions
using SurfaceDimensions = std::map<gl::ContextID, gl::Extents>;

class FrameCapture final : angle::NonCopyable
{
  public:
    FrameCapture();
    ~FrameCapture();

    void captureCall(const gl::Context *context, CallCapture &&call, bool isCallValid);
    void checkForCaptureTrigger();
    void onEndFrame(const gl::Context *context);
    void onDestroyContext(const gl::Context *context);
    void onMakeCurrent(const gl::Context *context, const egl::Surface *drawSurface);
    bool enabled() const { return mEnabled; }

    bool isCapturing() const;
    void replay(gl::Context *context);
    uint32_t getFrameCount() const;

    // Returns a frame index starting from "1" as the first frame.
    uint32_t getReplayFrameIndex() const;

    void trackBufferMapping(CallCapture *call,
                            gl::BufferID id,
                            GLintptr offset,
                            GLsizeiptr length,
                            bool writable);

    ResourceTracker &getResouceTracker() { return mResourceTracker; }

  private:
    void captureClientArraySnapshot(const gl::Context *context,
                                    size_t vertexCount,
                                    size_t instanceCount);
    void captureMappedBufferSnapshot(const gl::Context *context, const CallCapture &call);

    void copyCompressedTextureData(const gl::Context *context, const CallCapture &call);
    void captureCompressedTextureData(const gl::Context *context, const CallCapture &call);

    void reset();
    void maybeOverrideEntryPoint(const gl::Context *context, CallCapture &call);
    void maybeCapturePreCallUpdates(const gl::Context *context, CallCapture &call);
    void maybeCapturePostCallUpdates(const gl::Context *context);
    void maybeCaptureDrawArraysClientData(const gl::Context *context,
                                          CallCapture &call,
                                          size_t instanceCount);
    void maybeCaptureDrawElementsClientData(const gl::Context *context,
                                            CallCapture &call,
                                            size_t instanceCount);

    static void ReplayCall(gl::Context *context,
                           ReplayContext *replayContext,
                           const CallCapture &call);

    void setCaptureActive() { mCaptureActive = true; }
    void setCaptureInactive() { mCaptureActive = false; }
    bool isCaptureActive() { return mCaptureActive; }

    std::vector<CallCapture> mSetupCalls;
    std::vector<CallCapture> mFrameCalls;

    // We save one large buffer of binary data for the whole CPP replay.
    // This simplifies a lot of file management.
    std::vector<uint8_t> mBinaryData;

    bool mEnabled = false;
    bool mSerializeStateEnabled;
    std::string mOutDirectory;
    std::string mCaptureLabel;
    bool mCompression;
    gl::AttribArray<int> mClientVertexArrayMap;
    uint32_t mFrameIndex;
    uint32_t mCaptureStartFrame;
    uint32_t mCaptureEndFrame;
    bool mIsFirstFrame   = true;
    bool mWroteIndexFile = false;
    SurfaceDimensions mDrawSurfaceDimensions;
    gl::AttribArray<size_t> mClientArraySizes;
    size_t mReadBufferSize;
    HasResourceTypeMap mHasResourceType;
    BufferDataMap mBufferDataMap;

    ResourceTracker mResourceTracker;

    // If you don't know which frame you want to start capturing at, use the capture trigger.
    // Initialize it to the number of frames you want to capture, and then clear the value to 0 when
    // you reach the content you want to capture. Currently only available on Android.
    uint32_t mCaptureTrigger;

    bool mCaptureActive = false;
};

// Shared class for any items that need to be tracked by FrameCapture across shared contexts
class FrameCaptureShared final : angle::NonCopyable
{
  public:
    FrameCaptureShared();
    ~FrameCaptureShared();

    const std::string &getShaderSource(gl::ShaderProgramID id) const;
    void setShaderSource(gl::ShaderProgramID id, std::string sources);

    const ProgramSources &getProgramSources(gl::ShaderProgramID id) const;
    void setProgramSources(gl::ShaderProgramID id, ProgramSources sources);

    // Load data from a previously stored texture level
    const std::vector<uint8_t> &retrieveCachedTextureLevel(gl::TextureID id,
                                                           gl::TextureTarget target,
                                                           GLint level);

    // Create new texture level data and copy the source into it
    void copyCachedTextureLevel(const gl::Context *context,
                                gl::TextureID srcID,
                                GLint srcLevel,
                                gl::TextureID dstID,
                                GLint dstLevel,
                                const CallCapture &call);

    // Create the location that should be used to cache texture level data
    std::vector<uint8_t> &getCachedTextureLevelData(gl::Texture *texture,
                                                    gl::TextureTarget target,
                                                    GLint level,
                                                    EntryPoint entryPoint);

    // Remove any cached texture levels on deletion
    void deleteCachedTextureLevelData(gl::TextureID id);

  private:
    // Cache most recently compiled and linked sources.
    ShaderSourceMap mCachedShaderSource;
    ProgramSourceMap mCachedProgramSources;

    // Cache a shadow copy of texture level data
    TextureLevels mCachedTextureLevels;
    TextureLevelDataMap mCachedTextureLevelData;
};

template <typename CaptureFuncT, typename... ArgsT>
void CaptureCallToFrameCapture(CaptureFuncT captureFunc,
                               bool isCallValid,
                               gl::Context *context,
                               ArgsT... captureParams)
{
    FrameCapture *frameCapture = context->getFrameCapture();
    if (!frameCapture->isCapturing())
    {
        return;
    }

    CallCapture call = captureFunc(context->getState(), isCallValid, captureParams...);

    frameCapture->captureCall(context, std::move(call), isCallValid);
}

template <typename T>
void ParamBuffer::addValueParam(const char *paramName, ParamType paramType, T paramValue)
{
    ParamCapture capture(paramName, paramType);
    InitParamValue(paramType, paramValue, &capture.value);
    mParamCaptures.emplace_back(std::move(capture));
}

template <typename T>
void ParamBuffer::addEnumParam(const char *paramName,
                               gl::GLenumGroup enumGroup,
                               ParamType paramType,
                               T paramValue)
{
    ParamCapture capture(paramName, paramType);
    InitParamValue(paramType, paramValue, &capture.value);
    capture.enumGroup = enumGroup;
    mParamCaptures.emplace_back(std::move(capture));
}

std::ostream &operator<<(std::ostream &os, const ParamCapture &capture);

// Pointer capture helpers.
void CaptureMemory(const void *source, size_t size, ParamCapture *paramCapture);
void CaptureString(const GLchar *str, ParamCapture *paramCapture);
void CaptureStringLimit(const GLchar *str, uint32_t limit, ParamCapture *paramCapture);
void CaptureVertexPointerGLES1(const gl::State &glState,
                               gl::ClientVertexArrayType type,
                               const void *pointer,
                               ParamCapture *paramCapture);

gl::Program *GetProgramForCapture(const gl::State &glState, gl::ShaderProgramID handle);

// For GetIntegerv, GetFloatv, etc.
void CaptureGetParameter(const gl::State &glState,
                         GLenum pname,
                         size_t typeSize,
                         ParamCapture *paramCapture);

void CaptureGetActiveUniformBlockivParameters(const gl::State &glState,
                                              gl::ShaderProgramID handle,
                                              gl::UniformBlockIndex uniformBlockIndex,
                                              GLenum pname,
                                              ParamCapture *paramCapture);

template <typename T>
void CaptureClearBufferValue(GLenum buffer, const T *value, ParamCapture *paramCapture)
{
    // Per the spec, color buffers have a vec4, the rest a single value
    uint32_t valueSize = (buffer == GL_COLOR) ? 4 : 1;
    CaptureMemory(value, valueSize * sizeof(T), paramCapture);
}

void CaptureGenHandlesImpl(GLsizei n, GLuint *handles, ParamCapture *paramCapture);

template <typename T>
void CaptureGenHandles(GLsizei n, T *handles, ParamCapture *paramCapture)
{
    CaptureGenHandlesImpl(n, reinterpret_cast<GLuint *>(handles), paramCapture);
}

void CaptureShaderStrings(GLsizei count,
                          const GLchar *const *strings,
                          const GLint *length,
                          ParamCapture *paramCapture);

template <ParamType ParamT, typename T>
void WriteParamValueReplay(std::ostream &os, const CallCapture &call, T value);

template <>
void WriteParamValueReplay<ParamType::TGLboolean>(std::ostream &os,
                                                  const CallCapture &call,
                                                  GLboolean value);

template <>
void WriteParamValueReplay<ParamType::TvoidConstPointer>(std::ostream &os,
                                                         const CallCapture &call,
                                                         const void *value);

template <>
void WriteParamValueReplay<ParamType::TGLDEBUGPROCKHR>(std::ostream &os,
                                                       const CallCapture &call,
                                                       GLDEBUGPROCKHR value);

template <>
void WriteParamValueReplay<ParamType::TGLDEBUGPROC>(std::ostream &os,
                                                    const CallCapture &call,
                                                    GLDEBUGPROC value);

template <>
void WriteParamValueReplay<ParamType::TBufferID>(std::ostream &os,
                                                 const CallCapture &call,
                                                 gl::BufferID value);

template <>
void WriteParamValueReplay<ParamType::TFenceNVID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::FenceNVID value);

template <>
void WriteParamValueReplay<ParamType::TFramebufferID>(std::ostream &os,
                                                      const CallCapture &call,
                                                      gl::FramebufferID value);

template <>
void WriteParamValueReplay<ParamType::TMemoryObjectID>(std::ostream &os,
                                                       const CallCapture &call,
                                                       gl::MemoryObjectID value);

template <>
void WriteParamValueReplay<ParamType::TProgramPipelineID>(std::ostream &os,
                                                          const CallCapture &call,
                                                          gl::ProgramPipelineID value);

template <>
void WriteParamValueReplay<ParamType::TQueryID>(std::ostream &os,
                                                const CallCapture &call,
                                                gl::QueryID value);

template <>
void WriteParamValueReplay<ParamType::TRenderbufferID>(std::ostream &os,
                                                       const CallCapture &call,
                                                       gl::RenderbufferID value);

template <>
void WriteParamValueReplay<ParamType::TSamplerID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::SamplerID value);

template <>
void WriteParamValueReplay<ParamType::TSemaphoreID>(std::ostream &os,
                                                    const CallCapture &call,
                                                    gl::SemaphoreID value);

template <>
void WriteParamValueReplay<ParamType::TShaderProgramID>(std::ostream &os,
                                                        const CallCapture &call,
                                                        gl::ShaderProgramID value);

template <>
void WriteParamValueReplay<ParamType::TTextureID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::TextureID value);

template <>
void WriteParamValueReplay<ParamType::TTransformFeedbackID>(std::ostream &os,
                                                            const CallCapture &call,
                                                            gl::TransformFeedbackID value);

template <>
void WriteParamValueReplay<ParamType::TVertexArrayID>(std::ostream &os,
                                                      const CallCapture &call,
                                                      gl::VertexArrayID value);

template <>
void WriteParamValueReplay<ParamType::TUniformLocation>(std::ostream &os,
                                                        const CallCapture &call,
                                                        gl::UniformLocation value);

template <>
void WriteParamValueReplay<ParamType::TUniformBlockIndex>(std::ostream &os,
                                                          const CallCapture &call,
                                                          gl::UniformBlockIndex value);

template <>
void WriteParamValueReplay<ParamType::TGLsync>(std::ostream &os,
                                               const CallCapture &call,
                                               GLsync value);

// General fallback for any unspecific type.
template <ParamType ParamT, typename T>
void WriteParamValueReplay(std::ostream &os, const CallCapture &call, T value)
{
    os << value;
}
}  // namespace angle

template <typename T>
void CaptureTextureAndSamplerParameter_params(GLenum pname,
                                              const T *param,
                                              angle::ParamCapture *paramCapture)
{
    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        CaptureMemory(param, sizeof(T) * 4, paramCapture);
    }
    else
    {
        CaptureMemory(param, sizeof(T), paramCapture);
    }
}

#endif  // LIBANGLE_FRAME_CAPTURE_H_
