/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegRuntimeLinker_h__
#define __FFmpegRuntimeLinker_h__

#include "PlatformDecoderModule.h"
#include "PlatformEncoderModule.h"

namespace mozilla {

class FFmpegRuntimeLinker {
 public:
  static bool Init();
  static already_AddRefed<PlatformDecoderModule> CreateDecoder();
  static already_AddRefed<PlatformEncoderModule> CreateEncoder();
  enum LinkStatus {
    LinkStatus_INIT = 0,   // Never been linked.
    LinkStatus_SUCCEEDED,  // Found a usable library.
    // The following error statuses are sorted from most to least preferred
    // (i.e., if more than one happens, the top one is chosen.)
    LinkStatus_INVALID_FFMPEG_CANDIDATE,  // Found ffmpeg with unexpected
                                          // contents.
    LinkStatus_UNUSABLE_LIBAV57,         // Found LibAV 57, which we cannot use.
    LinkStatus_INVALID_LIBAV_CANDIDATE,  // Found libav with unexpected
                                         // contents.
    LinkStatus_OBSOLETE_FFMPEG,
    LinkStatus_OBSOLETE_LIBAV,
    LinkStatus_INVALID_CANDIDATE,  // Found some lib with unexpected contents.
    LinkStatus_NOT_FOUND,  // Haven't found any library with an expected name.
  };
  static LinkStatus LinkStatusCode() { return sLinkStatus; }
  static const char* LinkStatusString();
  // Library name to which the sLinkStatus applies, or "" if not applicable.
  static const char* LinkStatusLibraryName() { return sLinkStatusLibraryName; }

 private:
  static LinkStatus sLinkStatus;
  static const char* sLinkStatusLibraryName;
};

}  // namespace mozilla

#endif  // __FFmpegRuntimeLinker_h__
