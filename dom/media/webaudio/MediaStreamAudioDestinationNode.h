/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaStreamAudioDestinationNode_h_
#define MediaStreamAudioDestinationNode_h_

#include "AudioNode.h"

namespace mozilla::dom {

class AudioContext;
struct AudioNodeOptions;

class MediaStreamAudioDestinationNode final : public AudioNode {
 public:
  static already_AddRefed<MediaStreamAudioDestinationNode> Create(
      AudioContext& aAudioContext, const AudioNodeOptions& aOptions,
      ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamAudioDestinationNode,
                                           AudioNode)

  static already_AddRefed<MediaStreamAudioDestinationNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const AudioNodeOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  uint16_t NumberOfOutputs() const final { return 0; }

  void DestroyMediaTrack() override;

  DOMMediaStream* DOMStream() const { return mDOMStream; }

  const char* NodeType() const override {
    return "MediaStreamAudioDestinationNode";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

 private:
  explicit MediaStreamAudioDestinationNode(AudioContext* aContext);
  ~MediaStreamAudioDestinationNode() = default;

  RefPtr<DOMMediaStream> mDOMStream;
};

}  // namespace mozilla::dom

#endif
