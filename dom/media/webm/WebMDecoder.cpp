/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "WebMDemuxer.h"
#include "WebMReader.h"
#include "WebMDecoder.h"

namespace mozilla {

MediaDecoderStateMachine* WebMDecoder::CreateStateMachine()
{
  bool useFormatDecoder =
    Preferences::GetBool("media.format-reader.webm", true);
  nsRefPtr<MediaDecoderReader> reader = useFormatDecoder ?
      static_cast<MediaDecoderReader*>(new MediaFormatReader(this, new WebMDemuxer(GetResource()))) :
      new WebMReader(this);
  return new MediaDecoderStateMachine(this, reader);
}

} // namespace mozilla

