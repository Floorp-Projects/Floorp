/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFVPXRuntimeLinker_h__
#define __FFVPXRuntimeLinker_h__

#include "PlatformDecoderModule.h"
#include "PlatformEncoderModule.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/ThreadSafety.h"

struct FFmpegRDFTFuncs;

namespace mozilla {

class FFVPXRuntimeLinker {
 public:
  static bool Init() MOZ_EXCLUDES(sMutex);
  static already_AddRefed<PlatformDecoderModule> CreateDecoder();
  static already_AddRefed<PlatformEncoderModule> CreateEncoder();

  // Call (on any thread) after Init().
  static void GetRDFTFuncs(FFmpegRDFTFuncs* aOutFuncs);

 private:
  // Provide critical-section for Init() and sLinkStatus.
  static StaticMutex sMutex;

  // Set once on the main thread and then read from other threads.
  static enum LinkStatus {
    LinkStatus_INIT = 0,
    LinkStatus_FAILED,
    LinkStatus_SUCCEEDED
  } sLinkStatus MOZ_GUARDED_BY(sMutex);
};

}  // namespace mozilla

#endif /* __FFVPXRuntimeLinker_h__ */
