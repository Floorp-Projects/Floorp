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

class AudioContext;
struct MediaElementAudioSourceOptions;

class MediaElementAudioSourceNode final : public MediaStreamAudioSourceNode {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaElementAudioSourceNode,
                                           MediaStreamAudioSourceNode)
  static already_AddRefed<MediaElementAudioSourceNode> Create(
      AudioContext& aAudioContext,
      const MediaElementAudioSourceOptions& aOptions, ErrorResult& aRv);

  static already_AddRefed<MediaElementAudioSourceNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const MediaElementAudioSourceOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  const char* NodeType() const override {
    return "MediaElementAudioSourceNode";
  }

  const char* CrossOriginErrorString() const override {
    return "MediaElementAudioSourceNodeCrossOrigin";
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  HTMLMediaElement* MediaElement();

 private:
  explicit MediaElementAudioSourceNode(AudioContext* aContext,
                                       HTMLMediaElement* aElement);
  ~MediaElementAudioSourceNode() = default;

  void Destroy() override;

  // If AudioContext was not allowed to start, we would try to start it when
  // source starts.
  void ListenForAllowedToPlay(const MediaElementAudioSourceOptions& aOptions);

  MozPromiseRequestHolder<GenericNonExclusivePromise> mAllowedToPlayRequest;

  RefPtr<HTMLMediaElement> mElement;
};

}  // namespace dom
}  // namespace mozilla

#endif
