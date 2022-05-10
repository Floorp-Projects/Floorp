/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaStreamAudioSourceNode_h_
#define MediaStreamAudioSourceNode_h_

#include "AudioNode.h"
#include "AudioNodeEngine.h"
#include "DOMMediaStream.h"
#include "PrincipalChangeObserver.h"

namespace mozilla::dom {

class AudioContext;
struct MediaStreamAudioSourceOptions;

class MediaStreamAudioSourceNodeEngine final : public AudioNodeEngine {
 public:
  explicit MediaStreamAudioSourceNodeEngine(AudioNode* aNode)
      : AudioNodeEngine(aNode), mEnabled(false) {}

  bool IsEnabled() const { return mEnabled; }
  enum Parameters { ENABLE };
  void SetInt32Parameter(uint32_t aIndex, int32_t aValue) override {
    switch (aIndex) {
      case ENABLE:
        mEnabled = !!aValue;
        break;
      default:
        NS_ERROR("MediaStreamAudioSourceNodeEngine bad parameter index");
    }
  }

 private:
  bool mEnabled;
};

class MediaStreamAudioSourceNode
    : public AudioNode,
      public DOMMediaStream::TrackListener,
      public PrincipalChangeObserver<MediaStreamTrack> {
 public:
  static already_AddRefed<MediaStreamAudioSourceNode> Create(
      AudioContext& aContext, const MediaStreamAudioSourceOptions& aOptions,
      ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MediaStreamAudioSourceNode,
                                           AudioNode)

  static already_AddRefed<MediaStreamAudioSourceNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const MediaStreamAudioSourceOptions& aOptions, ErrorResult& aRv) {
    return Create(aAudioContext, aOptions, aRv);
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void DestroyMediaTrack() override;

  uint16_t NumberOfInputs() const override { return 0; }

  DOMMediaStream* GetMediaStream() { return mInputStream; }

  const char* NodeType() const override { return "MediaStreamAudioSourceNode"; }

  virtual const char* CrossOriginErrorString() const {
    return "MediaStreamAudioSourceNodeCrossOrigin";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  // Attaches to aTrack so that its audio content will be used as input.
  void AttachToTrack(const RefPtr<MediaStreamTrack>& aTrack, ErrorResult& aRv);

  // Detaches from the currently attached track if there is one.
  void DetachFromTrack();

  // Attaches to the first audio track in the MediaStream, when the tracks are
  // ordered by id.
  void AttachToRightTrack(const RefPtr<DOMMediaStream>& aMediaStream,
                          ErrorResult& aRv);

  // From DOMMediaStream::TrackListener.
  void NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack) override;
  void NotifyTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack) override;
  void NotifyAudible() override;

  // From PrincipalChangeObserver<MediaStreamTrack>.
  void PrincipalChanged(MediaStreamTrack* aMediaStreamTrack) override;

  // This allows implementing the correct behaviour for both
  // MediaElementAudioSourceNode and MediaStreamAudioSourceNode, that have most
  // of their behaviour shared.
  enum TrackChangeBehavior {
    // MediaStreamAudioSourceNode locks on the track it picked, and never
    // changes.
    LockOnTrackPicked,
    // MediaElementAudioSourceNode can change track, depending on what the
    // HTMLMediaElement does.
    FollowChanges
  };

 protected:
  MediaStreamAudioSourceNode(AudioContext* aContext,
                             TrackChangeBehavior aBehavior);
  void Init(DOMMediaStream& aMediaStream, ErrorResult& aRv);
  virtual void Destroy();
  virtual ~MediaStreamAudioSourceNode();

 private:
  const TrackChangeBehavior mBehavior;
  RefPtr<MediaInputPort> mInputPort;
  RefPtr<DOMMediaStream> mInputStream;

  // On construction we set this to the first audio track of mInputStream.
  RefPtr<MediaStreamTrack> mInputTrack;
};

}  // namespace mozilla::dom

#endif
