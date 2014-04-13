/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaElementAudioSourceNode_h_
#define MediaElementAudioSourceNode_h_

#include "MediaStreamAudioSourceNode.h"

namespace mozilla {
namespace dom {

class MediaElementAudioSourceNode : public MediaStreamAudioSourceNode
{
public:
  MediaElementAudioSourceNode(AudioContext* aContext,
                              DOMMediaStream* aStream);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual const char* NodeType() const
  {
    return "MediaElementAudioSourceNode";
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
};

}
}

#endif
