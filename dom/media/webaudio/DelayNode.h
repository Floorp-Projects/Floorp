/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DelayNode_h_
#define DelayNode_h_

#include "AudioNode.h"
#include "AudioParam.h"

namespace mozilla::dom {

class AudioContext;
struct DelayOptions;

class DelayNode final : public AudioNode {
 public:
  static already_AddRefed<DelayNode> Create(AudioContext& aAudioContext,
                                            const DelayOptions& aOptions,
                                            ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DelayNode, AudioNode)

  static already_AddRefed<DelayNode> Constructor(const GlobalObject& aGlobal,
                                                 AudioContext& aAudioContext,
                                                 const DelayOptions& aOptions,
                                                 ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  AudioParam* DelayTime() const { return mDelay; }

  const char* NodeType() const override { return "DelayNode"; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

 private:
  DelayNode(AudioContext* aContext, double aMaxDelay);
  ~DelayNode() = default;

  friend class DelayNodeEngine;

  RefPtr<AudioParam> mDelay;
};

}  // namespace mozilla::dom

#endif
