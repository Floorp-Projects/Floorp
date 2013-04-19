/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WMFDecoder_h_)
#define WMFDecoder_h_

#include "MediaDecoder.h"

namespace mozilla {

// Decoder that uses Windows Media Foundation to playback H.264/AAC in MP4
// and M4A files, and MP3 files. Playback is strictly limited to only those
// codecs.
class WMFDecoder : public MediaDecoder
{
public:

  virtual MediaDecoder* Clone() {
    if (!IsWMFEnabled()) {
      return nullptr;
    }
    return new WMFDecoder();
  }

  virtual MediaDecoderStateMachine* CreateStateMachine();

  // Returns true if aType is a MIME type that we render with the
  // Windows Media Foundation backend. If aCodecList is non null,
  // it is filled with a (static const) null-terminated list of strings
  // denoting the codecs we'll playback. Note that playback is strictly
  // limited to the codecs in this list.
  static bool GetSupportedCodecs(const nsACString& aType,
                                 char const *const ** aCodecList);

  // Loads the DLLs required by Windows Media Foundation. If this returns
  // failure, you can assume that WMF is not available on the user's system.
  static nsresult LoadDLLs();
  static void UnloadDLLs();

  // Returns true if the WMF backend is preffed on, and we're running on a
  // version of Windows which is likely to support WMF.
  static bool IsEnabled();

  // Returns true if MP3 decoding is enabled on this system. We block
  // MP3 playback on Windows 7 SP0, since it's crashy on that platform.
  static bool IsMP3Supported();
};

} // namespace mozilla

#endif
