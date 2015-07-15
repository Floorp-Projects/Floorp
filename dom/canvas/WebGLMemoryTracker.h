/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_MEMORY_TRACKER_H_
#define WEBGL_MEMORY_TRACKER_H_

#include "mozilla/StaticPtr.h"
#include "nsIMemoryReporter.h"

namespace mozilla {

class WebGLContext;

class WebGLMemoryTracker : public nsIMemoryReporter
{
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIMEMORYREPORTER

    WebGLMemoryTracker();
    static StaticRefPtr<WebGLMemoryTracker> sUniqueInstance;

    // Here we store plain pointers, not RefPtrs: we don't want the
    // WebGLMemoryTracker unique instance to keep alive all
    // WebGLContexts ever created.
    typedef nsTArray<const WebGLContext*> ContextsArrayType;
    ContextsArrayType mContexts;

    void InitMemoryReporter();

    static WebGLMemoryTracker* UniqueInstance();

    static ContextsArrayType& Contexts() { return UniqueInstance()->mContexts; }

    friend class WebGLContext;

  public:

    static void AddWebGLContext(const WebGLContext* c) {
        Contexts().AppendElement(c);
    }

    static void RemoveWebGLContext(const WebGLContext* c) {
        ContextsArrayType & contexts = Contexts();
        contexts.RemoveElement(c);
        if (contexts.IsEmpty()) {
            sUniqueInstance = nullptr;
        }
    }

  private:
    virtual ~WebGLMemoryTracker();

    static int64_t GetTextureMemoryUsed();

    static int64_t GetTextureCount();

    static int64_t GetBufferMemoryUsed();

    static int64_t GetBufferCacheMemoryUsed();

    static int64_t GetBufferCount();

    static int64_t GetRenderbufferMemoryUsed();

    static int64_t GetRenderbufferCount();

    static int64_t GetShaderSize();

    static int64_t GetShaderCount();

    static int64_t GetContextCount() {
        return Contexts().Length();
    }
};

} // namespace mozilla

#endif // WEBGL_MEMORY_TRACKER_H_
