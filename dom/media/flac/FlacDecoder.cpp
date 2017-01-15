/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlacDecoder.h"
#include "FlacDemuxer.h"
#include "MediaContentType.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MediaPrefs.h"

namespace mozilla {

MediaDecoder*
FlacDecoder::Clone(MediaDecoderOwner* aOwner)
{
  if (!IsEnabled()) {
    return nullptr;
  }

  return new FlacDecoder(aOwner);
}

MediaDecoderStateMachine*
FlacDecoder::CreateStateMachine()
{
  RefPtr<MediaDecoderReader> reader =
      new MediaFormatReader(this, new FlacDemuxer(GetResource()));
  return new MediaDecoderStateMachine(this, reader);
}

/* static */ bool
FlacDecoder::IsEnabled()
{
#ifdef MOZ_FFVPX
  return MediaPrefs::FlacEnabled();
#else
  // Until bug 1295886 is fixed.
  return false;
#endif
}

/* static */ bool
FlacDecoder::IsSupportedType(const MediaContentType& aContentType)
{
  return IsEnabled()
         && (aContentType.Type() == MEDIAMIMETYPE("audio/flac")
             || aContentType.Type() == MEDIAMIMETYPE("audio/x-flac")
             || aContentType.Type() == MEDIAMIMETYPE("application/x-flac"));
}

} // namespace mozilla
