/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectShowDecoder.h"
#include "DirectShowReader.h"
#include "DirectShowUtils.h"
#include "MediaDecoderStateMachine.h"
#include "mozilla/Preferences.h"
#include "mozilla/WindowsVersion.h"

namespace mozilla {

MediaDecoderStateMachine* DirectShowDecoder::CreateStateMachine()
{
  return new MediaDecoderStateMachine(this, new DirectShowReader(this));
}

/* static */
bool
DirectShowDecoder::GetSupportedCodecs(const nsACString& aType,
                                      char const *const ** aCodecList)
{
  if (!IsEnabled()) {
    return false;
  }

  static char const *const mp3AudioCodecs[] = {
    "mp3",
    nullptr
  };
  if (aType.EqualsASCII("audio/mpeg") ||
      aType.EqualsASCII("audio/mp3")) {
    if (aCodecList) {
      *aCodecList = mp3AudioCodecs;
    }
    return true;
  }

  return false;
}

/* static */
bool
DirectShowDecoder::IsEnabled()
{
  return CanDecodeMP3UsingDirectShow() &&
         Preferences::GetBool("media.directshow.enabled");
}

DirectShowDecoder::DirectShowDecoder(MediaDecoderOwner* aOwner)
  : MediaDecoder(aOwner)
{
  MOZ_COUNT_CTOR(DirectShowDecoder);
}

DirectShowDecoder::~DirectShowDecoder()
{
  MOZ_COUNT_DTOR(DirectShowDecoder);
}

} // namespace mozilla

