/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectShowDecoder.h"
#include "DirectShowReader.h"
#include "DirectShowUtils.h"
#include "MediaContentType.h"
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
DirectShowDecoder::GetSupportedCodecs(const MediaContentType& aType,
                                      MediaCodecs* aOutCodecs)
{
  if (!IsEnabled()) {
    return false;
  }

  if (aType.Type() == MEDIAMIMETYPE("audio/mpeg")
      || aType.Type() == MEDIAMIMETYPE("audio/mp3")) {
    if (aOutCodecs) {
      *aOutCodecs = MediaCodecs("mp3");
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
}

DirectShowDecoder::~DirectShowDecoder() = default;

} // namespace mozilla

