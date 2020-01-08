/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLMemoryTracker.h"

#include "HostWebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"

namespace mozilla {

NS_IMETHODIMP
WebGLMemoryTracker::CollectReports(nsIHandleReportCallback* aHandleReport,
                                   nsISupports* aData, bool) {
  MOZ_COLLECT_REPORT(
      "webgl-texture-memory", KIND_OTHER, UNITS_BYTES, GetTextureMemoryUsed(),
      "Memory used by WebGL textures. The OpenGL implementation is free to "
      "store these textures in either video memory or main memory. This "
      "measurement is only a lower bound, actual memory usage may be higher "
      "for example if the storage is strided.");

  MOZ_COLLECT_REPORT("webgl-texture-count", KIND_OTHER, UNITS_COUNT,
                     GetTextureCount(), "Number of WebGL textures.");

  MOZ_COLLECT_REPORT(
      "webgl-buffer-memory", KIND_OTHER, UNITS_BYTES, GetBufferMemoryUsed(),
      "Memory used by WebGL buffers. The OpenGL implementation is free to "
      "store these buffers in either video memory or main memory. This "
      "measurement is only a lower bound, actual memory usage may be higher "
      "for example if the storage is strided.");

  MOZ_COLLECT_REPORT(
      "explicit/webgl/buffer-cache-memory", KIND_HEAP, UNITS_BYTES,
      GetBufferCacheMemoryUsed(),
      "Memory used by WebGL buffer caches. The WebGL implementation caches "
      "the contents of element array buffers only. This adds up with the "
      "'webgl-buffer-memory' value, but contrary to it, this one represents "
      "bytes on the heap, not managed by OpenGL.");

  MOZ_COLLECT_REPORT("webgl-buffer-count", KIND_OTHER, UNITS_COUNT,
                     GetBufferCount(), "Number of WebGL buffers.");

  MOZ_COLLECT_REPORT(
      "webgl-renderbuffer-memory", KIND_OTHER, UNITS_BYTES,
      GetRenderbufferMemoryUsed(),
      "Memory used by WebGL renderbuffers. The OpenGL implementation is free "
      "to store these renderbuffers in either video memory or main memory. "
      "This measurement is only a lower bound, actual memory usage may be "
      "higher, for example if the storage is strided.");

  MOZ_COLLECT_REPORT("webgl-renderbuffer-count", KIND_OTHER, UNITS_COUNT,
                     GetRenderbufferCount(), "Number of WebGL renderbuffers.");

  MOZ_COLLECT_REPORT(
      "explicit/webgl/shader", KIND_HEAP, UNITS_BYTES, GetShaderSize(),
      "Combined size of WebGL shader ASCII sources and translation logs "
      "cached on the heap.");

  MOZ_COLLECT_REPORT("webgl-shader-count", KIND_OTHER, UNITS_COUNT,
                     GetShaderCount(), "Number of WebGL shaders.");

  MOZ_COLLECT_REPORT("webgl-context-count", KIND_OTHER, UNITS_COUNT,
                     GetContextCount(), "Number of WebGL contexts.");

  return NS_OK;
}

NS_IMPL_ISUPPORTS(WebGLMemoryTracker, nsIMemoryReporter)

StaticRefPtr<WebGLMemoryTracker> WebGLMemoryTracker::sUniqueInstance;

/*static*/
RefPtr<WebGLMemoryTracker> WebGLMemoryTracker::Create() {
  RefPtr<WebGLMemoryTracker> ret = new WebGLMemoryTracker;
  RegisterWeakMemoryReporter(ret);
  return ret;
}

WebGLMemoryTracker::WebGLMemoryTracker() = default;

WebGLMemoryTracker::~WebGLMemoryTracker() {
  UnregisterWeakMemoryReporter(this);
}

// -

/*static*/
int64_t WebGLMemoryTracker::GetBufferCount() {
  const auto& contexts = Get()->mContexts;
  int64_t result = 0;
  for (const auto& context : contexts) {
    result += context->mBufferMap.size();
  }
  return result;
}

/*static*/
int64_t WebGLMemoryTracker::GetBufferMemoryUsed() {
  const auto& contexts = Get()->mContexts;
  int64_t result = 0;
  for (const auto& context : contexts) {
    for (const auto& pair : context->mBufferMap) {
      const auto& buffer = *pair.second;
      result += buffer.mByteLength;
    }
  }
  return result;
}

int64_t WebGLMemoryTracker::GetBufferCacheMemoryUsed() {
  const auto& contexts = Get()->mContexts;
  int64_t result = 0;
  for (const auto& context : contexts) {
    for (const auto& pair : context->mBufferMap) {
      const auto& buffer = *pair.second;
      if (buffer.mIndexCache) {
        result += buffer.mByteLength;
      }
      result += buffer.mIndexRanges.size() *
                sizeof(decltype(buffer.mIndexRanges)::value_type);
    }
  }
  return result;
}

MOZ_DEFINE_MALLOC_SIZE_OF(WebGLShaderMallocSizeOf)

int64_t WebGLMemoryTracker::GetShaderSize() {
  const auto& contexts = Get()->mContexts;
  int64_t result = 0;
  for (const auto& context : contexts) {
    for (const auto& pair : context->mShaderMap) {
      const auto& shader = pair.second;
      result += shader->SizeOfIncludingThis(WebGLShaderMallocSizeOf);
    }
  }
  return result;
}

/*static*/
int64_t WebGLMemoryTracker::GetTextureMemoryUsed() {
  const auto& contexts = Get()->mContexts;
  int64_t result = 0;
  for (const auto& context : contexts) {
    for (const auto& pair : context->mTextureMap) {
      const auto& texture = pair.second;
      result += texture->MemoryUsage();
    }
  }
  return result;
}

/*static*/
int64_t WebGLMemoryTracker::GetTextureCount() {
  const auto& contexts = Get()->mContexts;
  int64_t result = 0;
  for (const auto& context : contexts) {
    result += context->mTextureMap.size();
  }
  return result;
}

/*static*/
int64_t WebGLMemoryTracker::GetRenderbufferMemoryUsed() {
  const auto& contexts = Get()->mContexts;
  int64_t result = 0;
  for (const auto& context : contexts) {
    for (const auto& pair : context->mRenderbufferMap) {
      const auto& rb = pair.second;
      result += rb->MemoryUsage();
    }
  }
  return result;
}

/*static*/
int64_t WebGLMemoryTracker::GetRenderbufferCount() {
  const auto& contexts = Get()->mContexts;
  int64_t result = 0;
  for (const auto& context : contexts) {
    result += context->mRenderbufferMap.size();
  }
  return result;
}

/*static*/
int64_t WebGLMemoryTracker::GetShaderCount() {
  const auto& contexts = Get()->mContexts;
  int64_t result = 0;
  for (const auto& context : contexts) {
    result += context->mShaderMap.size();
  }
  return result;
}

}  // namespace mozilla
