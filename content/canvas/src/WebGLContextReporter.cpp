/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLMemoryTracker.h"
#include "nsIMemoryReporter.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(WebGLMemoryPressureObserver, nsIObserver)

class WebGLMemoryReporter MOZ_FINAL : public MemoryMultiReporter
{
public:
    WebGLMemoryReporter()
        : MemoryMultiReporter("webgl")
    {}

    NS_IMETHOD CollectReports(nsIMemoryReporterCallback* aCb,
                              nsISupports* aClosure);
};

NS_IMETHODIMP
WebGLMemoryReporter::CollectReports(nsIMemoryReporterCallback* aCb,
                                    nsISupports* aClosure)
{
#define REPORT(_path, _kind, _units, _amount, _desc)                          \
    do {                                                                      \
      nsresult rv;                                                            \
      rv = aCb->Callback(EmptyCString(), NS_LITERAL_CSTRING(_path), _kind,    \
                         _units, _amount, NS_LITERAL_CSTRING(_desc),          \
                         aClosure);                                           \
      NS_ENSURE_SUCCESS(rv, rv);                                              \
    } while (0)

    REPORT("webgl-texture-memory",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_BYTES,
           WebGLMemoryTracker::GetTextureMemoryUsed(),
           "Memory used by WebGL textures.The OpenGL"
           " implementation is free to store these textures in either video"
           " memory or main memory. This measurement is only a lower bound,"
           " actual memory usage may be higher for example if the storage"
           " is strided.");

    REPORT("webgl-texture-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryTracker::GetTextureCount(),
           "Number of WebGL textures.");

    REPORT("webgl-buffer-memory",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_BYTES,
           WebGLMemoryTracker::GetBufferMemoryUsed(),
           "Memory used by WebGL buffers. The OpenGL"
           " implementation is free to store these buffers in either video"
           " memory or main memory. This measurement is only a lower bound,"
           " actual memory usage may be higher for example if the storage"
           " is strided.");

    REPORT("explicit/webgl/buffer-cache-memory",
           nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES,
           WebGLMemoryTracker::GetBufferCacheMemoryUsed(),
           "Memory used by WebGL buffer caches. The WebGL"
           " implementation caches the contents of element array buffers"
           " only.This adds up with the webgl-buffer-memory value, but"
           " contrary to it, this one represents bytes on the heap,"
           " not managed by OpenGL.");

    REPORT("webgl-buffer-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryTracker::GetBufferCount(),
           "Number of WebGL buffers.");

    REPORT("webgl-renderbuffer-memory",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_BYTES,
           WebGLMemoryTracker::GetRenderbufferMemoryUsed(),
           "Memory used by WebGL renderbuffers. The OpenGL"
           " implementation is free to store these renderbuffers in either"
           " video memory or main memory. This measurement is only a lower"
           " bound, actual memory usage may be higher for example if the"
           " storage is strided.");

    REPORT("webgl-renderbuffer-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryTracker::GetRenderbufferCount(),
           "Number of WebGL renderbuffers.");

    REPORT("explicit/webgl/shader",
           nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES,
           WebGLMemoryTracker::GetShaderSize(),
           "Combined size of WebGL shader ASCII sources and translation"
           " logs cached on the heap.");

    REPORT("webgl-shader-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryTracker::GetShaderCount(),
           "Number of WebGL shaders.");

    REPORT("webgl-context-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryTracker::GetContextCount(),
           "Number of WebGL contexts.");

#undef REPORT

    return NS_OK;
}

NS_IMPL_ISUPPORTS1(WebGLMemoryTracker, nsISupports)

StaticRefPtr<WebGLMemoryTracker> WebGLMemoryTracker::sUniqueInstance;

WebGLMemoryTracker* WebGLMemoryTracker::UniqueInstance()
{
    if (!sUniqueInstance) {
        sUniqueInstance = new WebGLMemoryTracker;
    }
    return sUniqueInstance;
}

WebGLMemoryTracker::WebGLMemoryTracker()
{
    mReporter = new WebGLMemoryReporter;
    NS_RegisterMemoryReporter(mReporter);
}

WebGLMemoryTracker::~WebGLMemoryTracker()
{
    NS_UnregisterMemoryReporter(mReporter);
}

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(WebGLBufferMallocSizeOf)

int64_t
WebGLMemoryTracker::GetBufferCacheMemoryUsed() {
    const ContextsArrayType & contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLBuffer *buffer = contexts[i]->mBuffers.getFirst();
             buffer;
             buffer = buffer->getNext())
        {
            if (buffer->Target() == LOCAL_GL_ELEMENT_ARRAY_BUFFER)
                result += buffer->SizeOfIncludingThis(WebGLBufferMallocSizeOf);
        }
    }
    return result;
}

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(WebGLShaderMallocSizeOf)

int64_t
WebGLMemoryTracker::GetShaderSize() {
    const ContextsArrayType & contexts = Contexts();
    int64_t result = 0;
    for(size_t i = 0; i < contexts.Length(); ++i) {
        for (const WebGLShader *shader = contexts[i]->mShaders.getFirst();
             shader;
             shader = shader->getNext())
        {
            result += shader->SizeOfIncludingThis(WebGLShaderMallocSizeOf);
        }
    }
    return result;
}
