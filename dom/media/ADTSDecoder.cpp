/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ADTSDecoder.h"
#include "ADTSDemuxer.h"
#include "MediaContainerType.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "PDMFactory.h"

namespace mozilla {

ChannelMediaDecoder*
ADTSDecoder::Clone(MediaDecoderInit& aInit)
{
  if (!IsEnabled())
    return nullptr;

  return new ADTSDecoder(aInit);
}

MediaDecoderStateMachine*
ADTSDecoder::CreateStateMachine()
{
  MediaFormatReaderInit init(this);
  init.mCrashHelper = GetOwner()->CreateGMPCrashHelper();
  mReader = new MediaFormatReader(init, new ADTSDemuxer(mResource));
  return new MediaDecoderStateMachine(this, mReader);
}

/* static */ bool
ADTSDecoder::IsEnabled()
{
  RefPtr<PDMFactory> platform = new PDMFactory();
  return platform->SupportsMimeType(NS_LITERAL_CSTRING("audio/mp4a-latm"),
                                    /* DecoderDoctorDiagnostics* */ nullptr);
}

/* static */ bool
ADTSDecoder::IsSupportedType(const MediaContainerType& aContainerType)
{
  if (aContainerType.Type() == MEDIAMIMETYPE("audio/aac")
      || aContainerType.Type() == MEDIAMIMETYPE("audio/aacp")
      || aContainerType.Type() == MEDIAMIMETYPE("audio/x-aac")) {
    return
      IsEnabled()
      && (aContainerType.ExtendedType().Codecs().IsEmpty()
          || aContainerType.ExtendedType().Codecs() == "aac");
  }

  return false;
}

} // namespace mozilla
