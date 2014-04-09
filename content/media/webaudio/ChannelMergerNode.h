/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChannelMergerNode_h_
#define ChannelMergerNode_h_

#include "AudioNode.h"

namespace mozilla {
namespace dom {

class AudioContext;

class ChannelMergerNode : public AudioNode
{
public:
  ChannelMergerNode(AudioContext* aContext,
                    uint16_t aInputCount);

  NS_DECL_ISUPPORTS_INHERITED

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual uint16_t NumberOfInputs() const MOZ_OVERRIDE { return mInputCount; }

private:
  const uint16_t mInputCount;
};

}
}

#endif

