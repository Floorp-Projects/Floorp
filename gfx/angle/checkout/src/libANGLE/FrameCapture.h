//
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
#include "libANGLE/frame_capture_utils_autogen.h"

#include <tuple>

namespace angle
{
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
    std::vector<std::vector<uint8_t>> data;
    int arrayClientPointerIndex = -1;
    size_t readBufferSize       = 0;
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

    ParamCapture &getParam(const char *paramName, ParamType paramType, int index);

    void addParam(ParamCapture &&param);
    bool hasClientArrayData() const { return mClientArrayDataParam != -1; }
    ParamCapture &getClientArrayPointerParameter();
    size_t getReadBufferSize() const { return mReadBufferSize; }

    const std::vector<ParamCapture> &getParamCaptures() const { return mParamCaptures; }

  private:
    std::vector<ParamCapture> mParamCaptures;
    int mClientArrayDataParam = -1;
    size_t mReadBufferSize    = 0;
};

struct CallCapture
{
    CallCapture(const char *nameIn, ParamBuffer &&paramsIn);
    ~CallCapture();

    CallCapture(CallCapture &&other);
    CallCapture &operator=(CallCapture &&other);

    std::string name;
    ParamBuffer params;
};

class FrameCapture final : angle::NonCopyable
{
  public:
    FrameCapture();
    ~FrameCapture();

    void captureCall(const gl::Context *context, CallCapture &&call);
    void onEndFrame();
    bool enabled() const;

  private:
    // <CallName, ParamName>
    using Counter = std::tuple<std::string, std::string>;

    void captureClientArraySnapshot(const gl::Context *context,
                                    size_t vertexCount,
                                    size_t instanceCount);

    void writeCallReplay(const CallCapture &call,
                         std::ostream &out,
                         std::ostream &header,
                         std::vector<uint8_t> *binaryData);
    void reset();
    int getAndIncrementCounter(const std::string &callName, const std::string &paramName);
    bool anyClientArray() const;
    void saveCapturedFrameAsCpp();

    std::vector<CallCapture> mCalls;
    gl::AttribArray<int> mClientVertexArrayMap;
    size_t mFrameIndex;
    gl::AttribArray<size_t> mClientArraySizes;
    std::map<Counter, int> mDataCounters;
    size_t mReadBufferSize;
};

template <typename CaptureFuncT, typename ValidationFuncT, typename... ArgsT>
void CaptureCallToFrameCapture(const char *entryPointName,
                               CaptureFuncT captureFunc,
                               ValidationFuncT validationFunc,
                               gl::Context *context,
                               ArgsT... captureParams)
{
    FrameCapture *frameCapture = context->getFrameCapture();
    if (!frameCapture->enabled())
        return;

    bool isCallValid = validationFunc(context, captureParams...);
    CallCapture call = captureFunc(context, isCallValid, captureParams...);
    frameCapture->captureCall(context, std::move(call));
}

template <typename T>
void ParamBuffer::addValueParam(const char *paramName, ParamType paramType, T paramValue)
{
    ParamCapture capture(paramName, paramType);
    InitParamValue(paramType, paramValue, &capture.value);
    mParamCaptures.emplace_back(std::move(capture));
}

std::ostream &operator<<(std::ostream &os, const ParamCapture &capture);

// Pointer capture helpers.
void CaptureMemory(const void *source, size_t size, ParamCapture *paramCapture);
void CaptureString(const GLchar *str, ParamCapture *paramCapture);

template <ParamType ParamT, typename T>
void WriteParamValueToStream(std::ostream &os, T value);

template <>
void WriteParamValueToStream<ParamType::TGLboolean>(std::ostream &os, GLboolean value);

template <>
void WriteParamValueToStream<ParamType::TvoidConstPointer>(std::ostream &os, const void *value);

template <>
void WriteParamValueToStream<ParamType::TGLDEBUGPROCKHR>(std::ostream &os, GLDEBUGPROCKHR value);

template <>
void WriteParamValueToStream<ParamType::TGLDEBUGPROC>(std::ostream &os, GLDEBUGPROC value);

// General fallback for any unspecific type.
template <ParamType ParamT, typename T>
void WriteParamValueToStream(std::ostream &os, T value)
{
    os << value;
}
}  // namespace angle

#endif  // LIBANGLE_FRAME_CAPTURE_H_
