/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_BUFFER_H_
#define WEBGL_BUFFER_H_

#include <map>

#include "CacheInvalidator.h"
#include "GLDefs.h"
#include "WebGLObjectModel.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLBuffer final : public WebGLContextBoundObject {
  friend class WebGLContext;
  friend class WebGL2Context;
  friend class WebGLMemoryTracker;
  friend class WebGLTexture;

  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(WebGLBuffer, override)

 public:
  enum class Kind { Undefined, ElementArray, OtherData };

  WebGLBuffer(WebGLContext* webgl, GLuint buf);

  void SetContentAfterBind(GLenum target);
  Kind Content() const { return mContent; }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  GLenum Usage() const { return mUsage; }
  size_t ByteLength() const { return mByteLength; }

  Maybe<uint32_t> GetIndexedFetchMaxVert(GLenum type, uint64_t byteOffset,
                                         uint32_t indexCount) const;
  bool ValidateRange(size_t byteOffset, size_t byteLen) const;

  bool ValidateCanBindToTarget(GLenum target);
  void BufferData(GLenum target, uint64_t size, const void* data, GLenum usage);
  void BufferSubData(GLenum target, uint64_t dstByteOffset, uint64_t dataLen,
                     const void* data) const;

  ////

  const GLenum mGLName;

 protected:
  ~WebGLBuffer() override;

  void InvalidateCacheRange(uint64_t byteOffset, uint64_t byteLength) const;

  Kind mContent = Kind::Undefined;
  GLenum mUsage = LOCAL_GL_STATIC_DRAW;
  size_t mByteLength = 0;
  mutable uint64_t mLastUpdateFenceId = 0;

  struct IndexRange final {
    GLenum type;
    uint64_t byteOffset;
    uint32_t indexCount;

    bool operator<(const IndexRange& x) const {
      if (type != x.type) return type < x.type;

      if (byteOffset != x.byteOffset) return byteOffset < x.byteOffset;

      return indexCount < x.indexCount;
    }
  };

  UniqueBuffer mIndexCache;
  mutable std::map<IndexRange, Maybe<uint32_t>> mIndexRanges;

 public:
  CacheInvalidator mFetchInvalidator;

  void ResetLastUpdateFenceId() const;
};

}  // namespace mozilla

#endif  // WEBGL_BUFFER_H_
