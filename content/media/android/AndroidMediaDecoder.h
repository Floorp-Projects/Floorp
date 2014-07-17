/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaPluginDecoder_h_)
#define MediaPluginDecoder_h_

#include "MediaDecoder.h"
#include "MediaPluginDecoder.h"

namespace mozilla {

class MediaPluginDecoder : public MediaDecoder
{
  nsCString mType;
public:
  MediaPluginDecoder(const nsACString& aType);

  const nsresult GetContentType(nsACString& aType) const {
    aType = mType;
    return NS_OK;
  }

  virtual MediaDecoder* Clone() { return new MediaPluginDecoder(mType); }
  virtual MediaDecoderStateMachine* CreateStateMachine();
};

} // namespace mozilla

#endif
