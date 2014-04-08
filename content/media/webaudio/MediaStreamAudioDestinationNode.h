/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaStreamAudioDestinationNode_h_
#define MediaStreamAudioDestinationNode_h_

#include "AudioNode.h"

namespace mozilla {
namespace dom {

class MediaStreamAudioDestinationNode : public AudioNode
{
public:
  explicit MediaStreamAudioDestinationNode(AudioContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamAudioDestinationNode, AudioNode)

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual uint16_t NumberOfOutputs() const MOZ_FINAL MOZ_OVERRIDE
  {
    return 0;
  }

  virtual void DestroyMediaStream() MOZ_OVERRIDE;

  DOMMediaStream* DOMStream() const
  {
    return mDOMStream;
  }

private:
  nsRefPtr<DOMMediaStream> mDOMStream;
  nsRefPtr<MediaInputPort> mPort;
};

}
}

#endif
