/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFVPXRuntimeLinker_h__
#define __FFVPXRuntimeLinker_h__

#include "PlatformDecoderModule.h"

struct FFmpegRDFTFuncs;

namespace mozilla {

class FFVPXRuntimeLinker {
 public:
  // Main thread only.
  static bool Init();
  // Main thread or after Init().
  static already_AddRefed<PlatformDecoderModule> Create();

  // Call (on any thread) after Init().
  static void GetRDFTFuncs(FFmpegRDFTFuncs* aOutFuncs);

 private:
  // Set once on the main thread and then read from other threads.
  static enum LinkStatus {
    LinkStatus_INIT = 0,
    LinkStatus_FAILED,
    LinkStatus_SUCCEEDED
  } sLinkStatus;
};

}  // namespace mozilla

#endif /* __FFVPXRuntimeLinker_h__ */
