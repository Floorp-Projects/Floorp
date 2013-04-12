/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "nsIMemoryReporter.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(WebGLMemoryPressureObserver, nsIObserver)

class WebGLMemoryMultiReporter MOZ_FINAL : public nsIMemoryMultiReporter 
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMEMORYMULTIREPORTER
};

NS_IMPL_ISUPPORTS1(WebGLMemoryMultiReporter, nsIMemoryMultiReporter)

NS_IMETHODIMP
WebGLMemoryMultiReporter::GetName(nsACString &aName)
{
  aName.AssignLiteral("webgl");
  return NS_OK;
}

NS_IMETHODIMP
WebGLMemoryMultiReporter::CollectReports(nsIMemoryMultiReporterCallback* aCb, 
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
           WebGLMemoryMultiReporterWrapper::GetTextureMemoryUsed(),
           "Memory used by WebGL textures.The OpenGL"
           " implementation is free to store these textures in either video"
           " memory or main memory. This measurement is only a lower bound,"
           " actual memory usage may be higher for example if the storage" 
           " is strided.");
   
    REPORT("webgl-texture-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryMultiReporterWrapper::GetTextureCount(), 
           "Number of WebGL textures.");

    REPORT("webgl-buffer-memory",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_BYTES,
           WebGLMemoryMultiReporterWrapper::GetBufferMemoryUsed(), 
           "Memory used by WebGL buffers. The OpenGL"
           " implementation is free to store these buffers in either video"
           " memory or main memory. This measurement is only a lower bound,"
           " actual memory usage may be higher for example if the storage"
           " is strided.");

    REPORT("explicit/webgl/buffer-cache-memory",
           nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES,
           WebGLMemoryMultiReporterWrapper::GetBufferCacheMemoryUsed(),
           "Memory used by WebGL buffer caches. The WebGL"
           " implementation caches the contents of element array buffers"
           " only.This adds up with the webgl-buffer-memory value, but"
           " contrary to it, this one represents bytes on the heap,"
           " not managed by OpenGL.");

    REPORT("webgl-buffer-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryMultiReporterWrapper::GetBufferCount(),
           "Number of WebGL buffers."); 
   
    REPORT("webgl-renderbuffer-memory",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_BYTES,
           WebGLMemoryMultiReporterWrapper::GetRenderbufferMemoryUsed(), 
           "Memory used by WebGL renderbuffers. The OpenGL"
           " implementation is free to store these renderbuffers in either"
           " video memory or main memory. This measurement is only a lower"
           " bound, actual memory usage may be higher for example if the"
           " storage is strided.");
   
    REPORT("webgl-renderbuffer-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryMultiReporterWrapper::GetRenderbufferCount(),
           "Number of WebGL renderbuffers.");
  
    REPORT("explicit/webgl/shader",
           nsIMemoryReporter::KIND_HEAP, nsIMemoryReporter::UNITS_BYTES,
           WebGLMemoryMultiReporterWrapper::GetShaderSize(), 
           "Combined size of WebGL shader ASCII sources and translation"
           " logs cached on the heap."); 

    REPORT("webgl-shader-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryMultiReporterWrapper::GetShaderCount(), 
           "Number of WebGL shaders."); 

    REPORT("webgl-context-count",
           nsIMemoryReporter::KIND_OTHER, nsIMemoryReporter::UNITS_COUNT,
           WebGLMemoryMultiReporterWrapper::GetContextCount(), 
           "Number of WebGL contexts."); 

#undef REPORT

    return NS_OK;
}

WebGLMemoryMultiReporterWrapper* WebGLMemoryMultiReporterWrapper::sUniqueInstance = nullptr;

WebGLMemoryMultiReporterWrapper* WebGLMemoryMultiReporterWrapper::UniqueInstance()
{
    if (!sUniqueInstance) {
        sUniqueInstance = new WebGLMemoryMultiReporterWrapper;
    }
    return sUniqueInstance;     
}

WebGLMemoryMultiReporterWrapper::WebGLMemoryMultiReporterWrapper()
{ 
    mReporter = new WebGLMemoryMultiReporter;   
    NS_RegisterMemoryMultiReporter(mReporter);
}

WebGLMemoryMultiReporterWrapper::~WebGLMemoryMultiReporterWrapper()
{
    NS_UnregisterMemoryMultiReporter(mReporter);
}

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(WebGLBufferMallocSizeOf)

int64_t 
WebGLMemoryMultiReporterWrapper::GetBufferCacheMemoryUsed() {
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
WebGLMemoryMultiReporterWrapper::GetShaderSize() {
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
