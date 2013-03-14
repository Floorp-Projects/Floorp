/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderStateMachine.h"
#include "GStreamerReader.h"
#include "GStreamerDecoder.h"
#include "GStreamerFormatHelper.h"

namespace mozilla {

MediaDecoderStateMachine* GStreamerDecoder::CreateStateMachine()
{
  return new MediaDecoderStateMachine(this, new GStreamerReader(this));
}

bool
GStreamerDecoder::CanHandleMediaType(const nsACString& aMIMEType,
                                     const nsAString* aCodecs)
{
  return GStreamerFormatHelper::Instance()->CanHandleMediaType(aMIMEType, aCodecs);
}

} // namespace mozilla

