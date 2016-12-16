/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GainNode_h_
#define GainNode_h_

#include "AudioNode.h"
#include "AudioParam.h"

namespace mozilla {
namespace dom {

class AudioContext;
struct GainOptions;

class GainNode final : public AudioNode
{
public:
  static already_AddRefed<GainNode>
  Create(AudioContext& aAudioContext, const GainOptions& aOptions,
         ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(GainNode, AudioNode)

  static already_AddRefed<GainNode>
  Constructor(const GlobalObject& aGlobal, AudioContext& aAudioContext,
              const GainOptions& aOptions, ErrorResult& aRv)
  {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  AudioParam* Gain() const
  {
    return mGain;
  }

  const char* NodeType() const override
  {
    return "GainNode";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

private:
  explicit GainNode(AudioContext* aContext);
  ~GainNode() = default;

  RefPtr<AudioParam> mGain;
};

} // namespace dom
} // namespace mozilla

#endif
