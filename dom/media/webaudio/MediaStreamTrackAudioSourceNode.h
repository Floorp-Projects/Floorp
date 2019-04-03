/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaStreamTrackAudioSourceNode_h_
#define MediaStreamTrackAudioSourceNode_h_

#include "AudioNode.h"
#include "AudioNodeEngine.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {

namespace dom {

class AudioContext;
struct MediaStreamTrackAudioSourceOptions;

class MediaStreamTrackAudioSourceNodeEngine final : public AudioNodeEngine {
 public:
  explicit MediaStreamTrackAudioSourceNodeEngine(AudioNode* aNode)
      : AudioNodeEngine(aNode), mEnabled(false) {}

  bool IsEnabled() const { return mEnabled; }
  enum Parameters { ENABLE };
  void SetInt32Parameter(uint32_t aIndex, int32_t aValue) override {
    switch (aIndex) {
      case ENABLE:
        mEnabled = !!aValue;
        break;
      default:
        NS_ERROR("MediaStreamTrackAudioSourceNodeEngine bad parameter index");
    }
  }

 private:
  bool mEnabled;
};

class MediaStreamTrackAudioSourceNode
    : public AudioNode,
      public PrincipalChangeObserver<MediaStreamTrack>,
      public SupportsWeakPtr<MediaStreamTrackAudioSourceNode> {
 public:
  static already_AddRefed<MediaStreamTrackAudioSourceNode> Create(
      AudioContext& aContext,
      const MediaStreamTrackAudioSourceOptions& aOptions, ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(MediaStreamTrackAudioSourceNode)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamTrackAudioSourceNode,
                                           AudioNode)

  static already_AddRefed<MediaStreamTrackAudioSourceNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const MediaStreamTrackAudioSourceOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void DestroyMediaStream() override;

  uint16_t NumberOfInputs() const override { return 0; }

  const char* NodeType() const override {
    return "MediaStreamTrackAudioSourceNode";
  }

  virtual const char* CrossOriginErrorString() const {
    return "MediaStreamTrackAudioSourceNodeCrossOrigin";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  // From PrincipalChangeObserver<MediaStreamTrack>.
  void PrincipalChanged(MediaStreamTrack* aMediaStreamTrack) override;

 protected:
  explicit MediaStreamTrackAudioSourceNode(AudioContext* aContext);
  void Init(MediaStreamTrack* aMediaStreamTrack, ErrorResult& aRv);
  void Destroy();
  virtual ~MediaStreamTrackAudioSourceNode();

  class TrackListener : public MediaStreamTrackConsumer {
   public:
    explicit TrackListener(MediaStreamTrackAudioSourceNode* aNode)
        : mNode(aNode) {}

    void NotifyEnded(MediaStreamTrack* aTrack) override {
      if (mNode) {
        mNode->MarkInactive();
        mNode->DestroyMediaStream();
        mNode = nullptr;
      }
    }

   private:
    WeakPtr<MediaStreamTrackAudioSourceNode> mNode;
  };

 private:
  RefPtr<MediaInputPort> mInputPort;
  RefPtr<MediaStreamTrack> mInputTrack;
  TrackListener mTrackListener;
};

}  // namespace dom
}  // namespace mozilla

#endif
