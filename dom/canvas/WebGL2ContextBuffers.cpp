/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "ClientWebGLContext.h"
#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLTransformFeedback.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Buffer objects

void WebGL2Context::CopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                                      uint64_t readOffset, uint64_t writeOffset,
                                      uint64_t size) const {
  const FuncScope funcScope(*this, "copyBufferSubData");
  if (IsContextLost()) return;

  const auto& readBuffer = ValidateBufferSelection(readTarget);
  if (!readBuffer) return;

  const auto& writeBuffer = ValidateBufferSelection(writeTarget);
  if (!writeBuffer) return;

  if (!CheckedInt<GLintptr>(readOffset).isValid() ||
      !CheckedInt<GLintptr>(writeOffset).isValid() ||
      !CheckedInt<GLsizeiptr>(size).isValid())
    return ErrorOutOfMemory("offset or size too large for platform.");

  const auto fnValidateOffsetSize = [&](const char* info, WebGLintptr offset,
                                        const WebGLBuffer* buffer) {
    const auto neededBytes = CheckedInt<uint64_t>(offset) + size;
    if (!neededBytes.isValid() || neededBytes.value() > buffer->ByteLength()) {
      ErrorInvalidValue("Invalid %s range.", info);
      return false;
    }
    return true;
  };

  if (!fnValidateOffsetSize("read", readOffset, readBuffer) ||
      !fnValidateOffsetSize("write", writeOffset, writeBuffer)) {
    return;
  }

  if (readBuffer == writeBuffer) {
    const bool separate =
        (readOffset + size <= writeOffset || writeOffset + size <= readOffset);
    if (!separate) {
      ErrorInvalidValue(
          "Ranges [readOffset, readOffset + size) and"
          " [writeOffset, writeOffset + size) overlap.");
      return;
    }
  }

  const auto& readType = readBuffer->Content();
  const auto& writeType = writeBuffer->Content();
  MOZ_ASSERT(readType != WebGLBuffer::Kind::Undefined);
  MOZ_ASSERT(writeType != WebGLBuffer::Kind::Undefined);
  if (writeType != readType) {
    ErrorInvalidOperation(
        "Can't copy %s data to %s data.",
        (readType == WebGLBuffer::Kind::OtherData) ? "other" : "element",
        (writeType == WebGLBuffer::Kind::OtherData) ? "other" : "element");
    return;
  }

  const ScopedLazyBind readBind(gl, readTarget, readBuffer);
  const ScopedLazyBind writeBind(gl, writeTarget, writeBuffer);
  gl->fCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset,
                         size);

  writeBuffer->ResetLastUpdateFenceId();
}

bool WebGL2Context::GetBufferSubData(GLenum target, uint64_t srcByteOffset,
                                     const Range<uint8_t>& dest) const {
  const FuncScope funcScope(*this, "getBufferSubData");
  if (IsContextLost()) return false;

  const auto& buffer = ValidateBufferSelection(target);
  if (!buffer) return false;

  const auto byteLen = dest.length();
  if (!buffer->ValidateRange(srcByteOffset, byteLen)) return false;

  ////

  if (!CheckedInt<GLintptr>(srcByteOffset).isValid() ||
      !CheckedInt<GLsizeiptr>(byteLen).isValid()) {
    ErrorOutOfMemory("offset or size too large for platform.");
    return false;
  }
  const GLsizeiptr glByteLen(byteLen);

  ////

  switch (buffer->mUsage) {
    case LOCAL_GL_STATIC_READ:
    case LOCAL_GL_STREAM_READ:
    case LOCAL_GL_DYNAMIC_READ:
      if (mCompletedFenceId < buffer->mLastUpdateFenceId) {
        GenerateWarning(
            "Reading from a buffer without checking for previous"
            " command completion likely causes pipeline stalls."
            " Please use FenceSync.");
      }
      break;
    default:
      GenerateWarning(
          "Reading from a buffer with usage other than *_READ"
          " causes pipeline stalls. Copy through a STREAM_READ buffer.");
      break;
  }

  ////

  const ScopedLazyBind readBind(gl, target, buffer);

  if (byteLen) {
    const bool isTF = (target == LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER);
    GLenum mapTarget = target;
    if (isTF) {
      gl->fBindTransformFeedback(LOCAL_GL_TRANSFORM_FEEDBACK, mEmptyTFO);
      gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, buffer->mGLName);
      mapTarget = LOCAL_GL_ARRAY_BUFFER;
    }

    const auto mappedBytes = gl->fMapBufferRange(
        mapTarget, srcByteOffset, glByteLen, LOCAL_GL_MAP_READ_BIT);
    memcpy(dest.begin().get(), mappedBytes, dest.length());
    gl->fUnmapBuffer(mapTarget);

    if (isTF) {
      const GLuint vbo = (mBoundArrayBuffer ? mBoundArrayBuffer->mGLName : 0);
      gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, vbo);
      const GLuint tfo =
          (mBoundTransformFeedback ? mBoundTransformFeedback->mGLName : 0);
      gl->fBindTransformFeedback(LOCAL_GL_TRANSFORM_FEEDBACK, tfo);
    }
  }
  return true;
}

}  // namespace mozilla
