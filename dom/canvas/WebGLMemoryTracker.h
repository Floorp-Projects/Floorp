/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_MEMORY_TRACKER_H_
#define WEBGL_MEMORY_TRACKER_H_

#include "nsIMemoryReporter.h"

namespace mozilla {

class WebGLMemoryTracker : public nsIMemoryReporter {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

 private:
  virtual ~WebGLMemoryTracker() = default;
};

}  // namespace mozilla

#endif  // WEBGL_MEMORY_TRACKER_H_
