/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsWebMDecoder_h_)
#define nsWebMDecoder_h_

#include "nsBuiltinDecoder.h"

class nsWebMDecoder : public nsBuiltinDecoder
{
public:
  virtual nsMediaDecoder* Clone() {
    if (!nsHTMLMediaElement::IsWebMEnabled()) {
      return nullptr;
    }
    return new nsWebMDecoder();
  }
  virtual nsDecoderStateMachine* CreateStateMachine();
};

#endif
