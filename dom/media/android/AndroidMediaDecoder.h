/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AndroidMediaDecoder_h_)
#define AndroidMediaDecoder_h_

#include "MediaDecoder.h"
#include "AndroidMediaDecoder.h"

namespace mozilla {

class AndroidMediaDecoder : public MediaDecoder
{
  nsCString mType;
public:
  AndroidMediaDecoder(MediaDecoderOwner* aOwner, const nsACString& aType);

  nsresult GetContentType(nsACString& aType) const {
    aType = mType;
    return NS_OK;
  }

  virtual MediaDecoder* Clone(MediaDecoderOwner* aOwner) {
    return new AndroidMediaDecoder(aOwner, mType);
  }
  virtual MediaDecoderStateMachine* CreateStateMachine();
};

} // namespace mozilla

#endif
