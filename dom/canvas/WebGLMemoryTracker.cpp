/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLMemoryTracker.h"

#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLVertexAttribData.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLUniformLocation.h"

namespace mozilla {

NS_IMETHODIMP
WebGLMemoryTracker::CollectReports(nsIHandleReportCallback* aHandleReport,
                                   nsISupports* aData, bool)
{
    MOZ_COLLECT_REPORT(
        "webgl-texture-memory", KIND_OTHER, UNITS_BYTES, GetTextureMemoryUsed(),
        "Memory used by WebGL textures. The OpenGL implementation is free to "
        "store these textures in either video memory or main memory. This "
        "measurement is only a lower bound, actual memory usage may be higher "
        "for example if the storage is strided.");

    MOZ_COLLECT_REPORT(
        "webgl-texture-count", KIND_OTHER, UNITS_COUNT, GetTextureCount(),
        "Number of WebGL textures.");

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

    MOZ_COLLECT_REPORT(
        "webgl-buffer-count", KIND_OTHER, UNITS_COUNT, GetBufferCount(),
        "Number of WebGL buffers.");

    MOZ_COLLECT_REPORT(
        "webgl-renderbuffer-memory", KIND_OTHER, UNITS_BYTES,
        GetRenderbufferMemoryUsed(),
        "Memory used by WebGL renderbuffers. The OpenGL implementation is free "
        "to store these renderbuffers in either video memory or main memory. "
        "This measurement is only a lower bound, actual memory usage may be "
        "higher, for example if the storage is strided.");

    MOZ_COLLECT_REPORT(
        "webgl-renderbuffer-count", KIND_OTHER, UNITS_COUNT,
        GetRenderbufferCount(),
        "Number of WebGL renderbuffers.");

    MOZ_COLLECT_REPORT(
        "explicit/webgl/shader", KIND_HEAP, UNITS_BYTES, GetShaderSize(),
        "Combined size of WebGL shader ASCII sources and translation logs "
        "cached on the heap.");

    MOZ_COLLECT_REPORT(
        "webgl-shader-count", KIND_OTHER, UNITS_COUNT, GetShaderCount(),
        "Number of WebGL shaders.");

    MOZ_COLLECT_REPORT(
        "webgl-context-count", KIND_OTHER, UNITS_COUNT, GetContextCount(),
        "Number of WebGL contexts.");

    return NS_OK;
}

NS_IMPL_ISUPPORTS(WebGLMemoryTracker, nsIMemoryReporter)

StaticRefPtr<WebGLMemoryTracker> WebGLMemoryTracker::sUniqueInstance;

WebGLMemoryTracker*
WebGLMemoryTracker::UniqueInstance()
{
    if (!sUniqueInstance) {
        sUniqueInstance = new WebGLMemoryTracker;
        sUniqueInstance->InitMemoryReporter();
    }
    return sUniqueInstance;
}

WebGLMemoryTracker::WebGLMemoryTracker()
{
}

void
WebGLMemoryTracker::InitMemoryReporter()
{
    RegisterWeakMemoryReporter(this);
}

WebGLMemoryTracker::~WebGLMemoryTracker()
{
    UnregisterWeakMemoryReporter(this);
}

MOZ_DEFINE_MALLOC_SIZE_OF(WebGLBufferMallocSizeOf)

int64_t
WebGLMemoryTracker::GetBufferCacheMemoryUsed()
{
    const ContextsArrayType& contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLBuffer* buffer = contexts[i]->mBuffers.getFirst();
             buffer;
             buffer = buffer->getNext())
        {
            if (buffer->Content() == WebGLBuffer::Kind::ElementArray) {
                result += buffer->SizeOfIncludingThis(WebGLBufferMallocSizeOf);
            }
        }
    }
    return result;
}

MOZ_DEFINE_MALLOC_SIZE_OF(WebGLShaderMallocSizeOf)

int64_t
WebGLMemoryTracker::GetShaderSize()
{
    const ContextsArrayType& contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLShader* shader = contexts[i]->mShaders.getFirst();
             shader;
             shader = shader->getNext())
        {
            result += shader->SizeOfIncludingThis(WebGLShaderMallocSizeOf);
        }
    }
    return result;
}

/*static*/ int64_t
WebGLMemoryTracker::GetTextureMemoryUsed()
{
    const ContextsArrayType & contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLTexture* texture = contexts[i]->mTextures.getFirst();
             texture;
             texture = texture->getNext())
        {
            result += texture->MemoryUsage();
        }
    }
    return result;
}

/*static*/ int64_t
WebGLMemoryTracker::GetTextureCount()
{
    const ContextsArrayType & contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLTexture* texture = contexts[i]->mTextures.getFirst();
             texture;
             texture = texture->getNext())
        {
            result++;
        }
    }
    return result;
}

/*static*/ int64_t
WebGLMemoryTracker::GetBufferMemoryUsed()
{
    const ContextsArrayType & contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLBuffer* buffer = contexts[i]->mBuffers.getFirst();
             buffer;
             buffer = buffer->getNext())
        {
            result += buffer->ByteLength();
        }
    }
    return result;
}

/*static*/ int64_t
WebGLMemoryTracker::GetBufferCount()
{
    const ContextsArrayType & contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLBuffer* buffer = contexts[i]->mBuffers.getFirst();
             buffer;
             buffer = buffer->getNext())
        {
            result++;
        }
    }
    return result;
}

/*static*/ int64_t
WebGLMemoryTracker::GetRenderbufferMemoryUsed()
{
    const ContextsArrayType & contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLRenderbuffer* rb = contexts[i]->mRenderbuffers.getFirst();
             rb;
             rb = rb->getNext())
        {
            result += rb->MemoryUsage();
        }
    }
    return result;
}

/*static*/ int64_t
WebGLMemoryTracker::GetRenderbufferCount()
{
    const ContextsArrayType & contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLRenderbuffer* rb = contexts[i]->mRenderbuffers.getFirst();
             rb;
             rb = rb->getNext())
        {
            result++;
        }
    }
    return result;
}

/*static*/ int64_t
WebGLMemoryTracker::GetShaderCount()
{
    const ContextsArrayType & contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLShader* shader = contexts[i]->mShaders.getFirst();
             shader;
             shader = shader->getNext())
        {
            result++;
        }
    }
    return result;
}

} // namespace mozilla
