/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsMediaPluginDecoder_h_)
#define nsMediaPluginDecoder_h_

#include "nsBuiltinDecoder.h"
#include "nsMediaPluginDecoder.h"

class nsMediaPluginDecoder : public nsBuiltinDecoder
{
  nsCString mType;
public:
  nsMediaPluginDecoder(const nsACString& aType);

  const nsresult GetContentType(nsACString& aType) const {
    aType = mType;
    return NS_OK;
  }

  virtual nsMediaDecoder* Clone() { return new nsMediaPluginDecoder(mType); }
  virtual nsDecoderStateMachine* CreateStateMachine();
};

#endif
