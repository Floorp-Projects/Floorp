/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamAudioDestinationNode.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MediaStreamAudioDestinationNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "AudioStreamTrack.h"
#include "DOMMediaStream.h"
#include "ForwardedInputTrack.h"

namespace mozilla::dom {

class AudioDestinationTrackSource final : public MediaStreamTrackSource {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioDestinationTrackSource,
                                           MediaStreamTrackSource)

  AudioDestinationTrackSource(MediaStreamAudioDestinationNode* aNode,
                              mozilla::MediaTrack* aInputTrack,
                              ProcessedMediaTrack* aTrack,
                              nsIPrincipal* aPrincipal)
      : MediaStreamTrackSource(
            aPrincipal, nsString(),
            // We pass 0 here because tracking ids are video only for now.
            TrackingId(TrackingId::Source::AudioDestinationNode, 0)),
        mTrack(aTrack),
        mPort(mTrack->AllocateInputPort(aInputTrack)),
        mNode(aNode) {}

  void Destroy() override {
    if (!mTrack->IsDestroyed()) {
      mTrack->Destroy();
      mPort->Destroy();
    }
    if (mNode) {
      mNode->DestroyMediaTrack();
      mNode = nullptr;
    }
  }

  MediaSourceEnum GetMediaSource() const override {
    return MediaSourceEnum::AudioCapture;
  }

  void Stop() override { Destroy(); }

  void Disable() override {}

  void Enable() override {}

  const RefPtr<ProcessedMediaTrack> mTrack;
  const RefPtr<MediaInputPort> mPort;

 private:
  ~AudioDestinationTrackSource() = default;

  RefPtr<MediaStreamAudioDestinationNode> mNode;
};

NS_IMPL_ADDREF_INHERITED(AudioDestinationTrackSource, MediaStreamTrackSource)
NS_IMPL_RELEASE_INHERITED(AudioDestinationTrackSource, MediaStreamTrackSource)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioDestinationTrackSource)
NS_INTERFACE_MAP_END_INHERITING(MediaStreamTrackSource)
NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioDestinationTrackSource,
                                   MediaStreamTrackSource, mNode)

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaStreamAudioDestinationNode, AudioNode,
                                   mDOMStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamAudioDestinationNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(MediaStreamAudioDestinationNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(MediaStreamAudioDestinationNode, AudioNode)

MediaStreamAudioDestinationNode::MediaStreamAudioDestinationNode(
    AudioContext* aContext)
    : AudioNode(aContext, 2, ChannelCountMode::Explicit,
                ChannelInterpretation::Speakers),
      mDOMStream(MakeAndAddRef<DOMMediaStream>(GetOwner())) {
  // Ensure an audio track with the correct ID is exposed to JS. If we can't get
  // a principal here because the document is not available, pass in a null
  // principal. This happens in edge cases when the document is being unloaded
  // and it does not matter too much to have something working as long as it's
  // not dangerous.
  nsCOMPtr<nsIPrincipal> principal = nullptr;
  if (aContext->GetParentObject()) {
    Document* doc = aContext->GetParentObject()->GetExtantDoc();
    principal = doc->NodePrincipal();
  }
  mTrack = AudioNodeTrack::Create(aContext, new AudioNodeEngine(this),
                                  AudioNodeTrack::EXTERNAL_OUTPUT,
                                  aContext->Graph());
  auto source = MakeRefPtr<AudioDestinationTrackSource>(
      this, mTrack,
      aContext->Graph()->CreateForwardedInputTrack(MediaSegment::AUDIO),
      principal);
  auto track = MakeRefPtr<AudioStreamTrack>(GetOwner(), source->mTrack, source);
  mDOMStream->AddTrackInternal(track);
}

/* static */
already_AddRefed<MediaStreamAudioDestinationNode>
MediaStreamAudioDestinationNode::Create(AudioContext& aAudioContext,
                                        const AudioNodeOptions& aOptions,
                                        ErrorResult& aRv) {
  // The spec has a pointless check here.  See
  // https://github.com/WebAudio/web-audio-api/issues/2149
  MOZ_RELEASE_ASSERT(!aAudioContext.IsOffline(), "Bindings messed up?");

  RefPtr<MediaStreamAudioDestinationNode> audioNode =
      new MediaStreamAudioDestinationNode(&aAudioContext);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return audioNode.forget();
}

size_t MediaStreamAudioDestinationNode::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  // Future:
  // - mDOMStream
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

size_t MediaStreamAudioDestinationNode::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void MediaStreamAudioDestinationNode::DestroyMediaTrack() {
  AudioNode::DestroyMediaTrack();
}

JSObject* MediaStreamAudioDestinationNode::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MediaStreamAudioDestinationNode_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
