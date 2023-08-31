/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientWebGLContext.h"

#include <limits>

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArray.h"

namespace mozilla {

RefPtr<WebGLBuffer>* WebGLContext::ValidateBufferSlot(GLenum target) {
  RefPtr<WebGLBuffer>* slot = nullptr;

  switch (target) {
    case LOCAL_GL_ARRAY_BUFFER:
      slot = &mBoundArrayBuffer;
      break;

    case LOCAL_GL_ELEMENT_ARRAY_BUFFER:
      slot = &(mBoundVertexArray->mElementArrayBuffer);
      break;
  }

  if (IsWebGL2()) {
    switch (target) {
      case LOCAL_GL_COPY_READ_BUFFER:
        slot = &mBoundCopyReadBuffer;
        break;

      case LOCAL_GL_COPY_WRITE_BUFFER:
        slot = &mBoundCopyWriteBuffer;
        break;

      case LOCAL_GL_PIXEL_PACK_BUFFER:
        slot = &mBoundPixelPackBuffer;
        break;

      case LOCAL_GL_PIXEL_UNPACK_BUFFER:
        slot = &mBoundPixelUnpackBuffer;
        break;

      case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
        slot = &mBoundTransformFeedbackBuffer;
        break;

      case LOCAL_GL_UNIFORM_BUFFER:
        slot = &mBoundUniformBuffer;
        break;
    }
  }

  if (!slot) {
    ErrorInvalidEnumInfo("target", target);
    return nullptr;
  }

  return slot;
}

WebGLBuffer* WebGLContext::ValidateBufferSelection(GLenum target) const {
  const auto& slot =
      const_cast<WebGLContext*>(this)->ValidateBufferSlot(target);
  if (!slot) return nullptr;
  const auto& buffer = *slot;

  if (!buffer) {
    ErrorInvalidOperation("Buffer for `target` is null.");
    return nullptr;
  }

  if (target == LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER) {
    if (mBoundTransformFeedback->IsActiveAndNotPaused()) {
      ErrorInvalidOperation(
          "Cannot select TRANSFORM_FEEDBACK_BUFFER when"
          " transform feedback is active and unpaused.");
      return nullptr;
    }
    const auto tfBuffers = std::vector<webgl::BufferAndIndex>{{
        {buffer},
    }};

    if (!ValidateBuffersForTf(tfBuffers)) return nullptr;
  } else {
    if (mBoundTransformFeedback && !ValidateBufferForNonTf(buffer, target))
      return nullptr;
  }

  return buffer.get();
}

IndexedBufferBinding* WebGLContext::ValidateIndexedBufferSlot(GLenum target,
                                                              GLuint index) {
  decltype(mIndexedUniformBufferBindings)* bindings;
  const char* maxIndexEnum;
  switch (target) {
    case LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER:
      if (mBoundTransformFeedback->mIsActive &&
          !mBoundTransformFeedback->mIsPaused) {
        ErrorInvalidOperation("Transform feedback active and not paused.");
        return nullptr;
      }
      bindings = &(mBoundTransformFeedback->mIndexedBindings);
      maxIndexEnum = "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS";
      break;

    case LOCAL_GL_UNIFORM_BUFFER:
      bindings = &mIndexedUniformBufferBindings;
      maxIndexEnum = "MAX_UNIFORM_BUFFER_BINDINGS";
      break;

    default:
      ErrorInvalidEnumInfo("target", target);
      return nullptr;
  }

  if (index >= bindings->size()) {
    ErrorInvalidValue("`index` >= %s.", maxIndexEnum);
    return nullptr;
  }

  return &(*bindings)[index];
}

////////////////////////////////////////

void WebGLContext::BindBuffer(GLenum target, WebGLBuffer* buffer) {
  FuncScope funcScope(*this, "bindBuffer");
  if (IsContextLost()) return;
  funcScope.mBindFailureGuard = true;

  if (buffer && !ValidateObject("buffer", *buffer)) return;

  const auto& slot = ValidateBufferSlot(target);
  if (!slot) return;

  if (buffer && !buffer->ValidateCanBindToTarget(target)) return;

  if (!IsBufferTargetLazilyBound(target)) {
    gl->fBindBuffer(target, buffer ? buffer->mGLName : 0);
  }

  *slot = buffer;
  if (buffer) {
    buffer->SetContentAfterBind(target);
  }

  funcScope.mBindFailureGuard = false;
}

////////////////////////////////////////

bool WebGLContext::ValidateIndexedBufferBinding(
    GLenum target, GLuint index, RefPtr<WebGLBuffer>** const out_genericBinding,
    IndexedBufferBinding** const out_indexedBinding) {
  *out_genericBinding = ValidateBufferSlot(target);
  if (!*out_genericBinding) return false;

  *out_indexedBinding = ValidateIndexedBufferSlot(target, index);
  if (!*out_indexedBinding) return false;

  if (target == LOCAL_GL_TRANSFORM_FEEDBACK_BUFFER &&
      mBoundTransformFeedback->mIsActive) {
    ErrorInvalidOperation(
        "Cannot update indexed buffer bindings on active"
        " transform feedback objects.");
    return false;
  }

  return true;
}

void WebGLContext::BindBufferRange(GLenum target, GLuint index,
                                   WebGLBuffer* buffer, uint64_t offset,
                                   uint64_t size) {
  FuncScope funcScope(*this, "bindBufferBase/Range");
  if (buffer && !ValidateObject("buffer", *buffer)) return;
  funcScope.mBindFailureGuard = true;

  RefPtr<WebGLBuffer>* genericBinding;
  IndexedBufferBinding* indexedBinding;
  if (!ValidateIndexedBufferBinding(target, index, &genericBinding,
                                    &indexedBinding)) {
    return;
  }

  if (buffer && !buffer->ValidateCanBindToTarget(target)) return;

  const auto& limits = Limits();
  auto err =
      CheckBindBufferRange(target, index, bool(buffer), offset, size, limits);
  if (err) return;

  ////

  bool needsPrebind = false;
  needsPrebind |= gl->IsANGLE();
#ifdef XP_MACOSX
  needsPrebind = true;
#endif

  if (gl->WorkAroundDriverBugs() && buffer && needsPrebind) {
    // BindBufferBase/Range will fail (on some drivers) if the buffer name has
    // never been bound. (GenBuffers makes a name, but BindBuffer initializes
    // that name as a real buffer object)
    gl->fBindBuffer(target, buffer->mGLName);
  }

  if (size) {
    gl->fBindBufferRange(target, index, buffer ? buffer->mGLName : 0, offset,
                         size);
  } else {
    gl->fBindBufferBase(target, index, buffer ? buffer->mGLName : 0);
  }

  if (buffer) {
    gl->fBindBuffer(target, 0);  // Reset generic.
  }

  ////

  *genericBinding = buffer;
  indexedBinding->mBufferBinding = buffer;
  indexedBinding->mRangeStart = offset;
  indexedBinding->mRangeSize = size;

  if (buffer) {
    buffer->SetContentAfterBind(target);
  }

  funcScope.mBindFailureGuard = false;
}

////////////////////////////////////////

void WebGLContext::BufferData(GLenum target, uint64_t dataLen,
                              const uint8_t* data, GLenum usage) const {
  // `data` may be null.
  const FuncScope funcScope(*this, "bufferData");
  if (IsContextLost()) return;

  const auto& buffer = ValidateBufferSelection(target);
  if (!buffer) return;

  buffer->BufferData(target, dataLen, data, usage);
}

////////////////////////////////////////

void WebGLContext::BufferSubData(GLenum target, uint64_t dstByteOffset,
                                 uint64_t dataLen, const uint8_t* data,
                                 bool unsynchronized) const {
  MOZ_ASSERT(data || !dataLen);
  const FuncScope funcScope(*this, "bufferSubData");
  if (IsContextLost()) return;

  const auto& buffer = ValidateBufferSelection(target);
  if (!buffer) return;
  buffer->BufferSubData(target, dstByteOffset, dataLen, data, unsynchronized);
}

////////////////////////////////////////

RefPtr<WebGLBuffer> WebGLContext::CreateBuffer() {
  const FuncScope funcScope(*this, "createBuffer");
  if (IsContextLost()) return nullptr;

  GLuint buf = 0;
  gl->fGenBuffers(1, &buf);

  return new WebGLBuffer(this, buf);
}

}  // namespace mozilla
