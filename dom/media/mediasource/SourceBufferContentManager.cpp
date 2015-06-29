/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBufferContentManager.h"
#include "TrackBuffer.h"
#include "TrackBuffersManager.h"

namespace mozilla {

#if defined(MOZ_GONK_MEDIACODEC) || defined(XP_WIN) || defined(MOZ_APPLEMEDIA) || defined(MOZ_FFMPEG)
#define MP4_READER_DORMANT_HEURISTIC
#else
#undef MP4_READER_DORMANT_HEURISTIC
#endif

already_AddRefed<SourceBufferContentManager>
SourceBufferContentManager::CreateManager(dom::SourceBuffer* aParent,
                                          MediaSourceDecoder* aParentDecoder,
                                          const nsACString &aType)
{
  nsRefPtr<SourceBufferContentManager> manager;
  bool useFormatReader =
    Preferences::GetBool("media.mediasource.format-reader", false);
  if (useFormatReader) {
    manager = new TrackBuffersManager(aParent, aParentDecoder, aType);
  } else {
    manager = new TrackBuffer(aParentDecoder, aType);
  }

  // Now that we know what type we're dealing with, enable dormant as needed.
#if defined(MP4_READER_DORMANT_HEURISTIC)
  if (aType.LowerCaseEqualsLiteral("video/mp4") ||
      aType.LowerCaseEqualsLiteral("audio/mp4") ||
      useFormatReader)
  {
    aParentDecoder->NotifyDormantSupported(Preferences::GetBool("media.decoder.heuristic.dormant.enabled", false));
  }
#endif


  return  manager.forget();
}

}
