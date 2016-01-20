/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DelayNode_h_
#define DelayNode_h_

#include "AudioNode.h"
#include "AudioParam.h"

namespace mozilla {
namespace dom {

class AudioContext;

class DelayNode final : public AudioNode
{
public:
  DelayNode(AudioContext* aContext, double aMaxDelay);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DelayNode, AudioNode)

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  AudioParam* DelayTime() const
  {
    return mDelay;
  }

  const char* NodeType() const override
  {
    return "DelayNode";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

protected:
  virtual ~DelayNode();

private:
  friend class DelayNodeEngine;

private:
  RefPtr<AudioParam> mDelay;
};

} // namespace dom
} // namespace mozilla

#endif

