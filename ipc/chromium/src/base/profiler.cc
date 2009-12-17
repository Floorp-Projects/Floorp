// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler.h"
#include "base/string_util.h"

// When actually using quantify, uncomment the following line.
//#define QUANTIFY

#ifdef QUANTIFY
// this #define is used to prevent people from directly using pure.h
// instead of profiler.h
#define PURIFY_PRIVATE_INCLUDE
#include "base/third_party/purify/pure.h"
#endif // QUANTIFY

namespace base {

void Profiler::StartRecording() {
#ifdef QUANTIFY
  QuantifyStartRecordingData();
#endif
}

void Profiler::StopRecording() {
#ifdef QUANTIFY
  QuantifyStopRecordingData();
#endif
}

void Profiler::ClearData() {
#ifdef QUANTIFY
  QuantifyClearData();
#endif
}

void Profiler::SetThreadName(const char *name) {
#ifdef QUANTIFY
  // make a copy since the Quantify function takes a char*, not const char*
  char buffer[512];
  base::snprintf(buffer, sizeof(buffer)-1, "%s", name);
  QuantifySetThreadName(buffer);
#endif
}

}  // namespace base
