/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_MEMORY_TRACKER_H_
#define WEBGL_MEMORY_TRACKER_H_

#include "mozilla/StaticPtr.h"
#include "nsIMemoryReporter.h"
#include <unordered_set>

namespace mozilla {

class HostWebGLContext;

class WebGLMemoryTracker : public nsIMemoryReporter {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

  static StaticRefPtr<WebGLMemoryTracker> sUniqueInstance;

  static RefPtr<WebGLMemoryTracker> Create();

  static RefPtr<WebGLMemoryTracker> Get() {
    if (!sUniqueInstance) {
      sUniqueInstance = WebGLMemoryTracker::Create().get();
    }
    return sUniqueInstance;
  }

  // Here we store plain pointers, not RefPtrs: we don't want the
  // WebGLMemoryTracker unique instance to keep alive all
  // WebGLContexts ever created.
  std::unordered_set<const HostWebGLContext*> mContexts;

 public:
  static void AddContext(const HostWebGLContext* c) {
    const auto tracker = Get();
    tracker->mContexts.insert(c);
  }

  static void RemoveContext(const HostWebGLContext* c) {
    const auto tracker = Get();
    tracker->mContexts.erase(c);
  }

 private:
  WebGLMemoryTracker();
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

  static int64_t GetContextCount() { return Get()->mContexts.size(); }
};

}  // namespace mozilla

#endif  // WEBGL_MEMORY_TRACKER_H_
