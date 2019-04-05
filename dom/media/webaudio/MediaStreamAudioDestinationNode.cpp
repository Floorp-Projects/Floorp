/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamAudioDestinationNode.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MediaStreamAudioDestinationNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "DOMMediaStream.h"
#include "MediaStreamTrack.h"
#include "TrackUnionStream.h"

namespace mozilla {
namespace dom {

class AudioDestinationTrackSource final : public MediaStreamTrackSource {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioDestinationTrackSource,
                                           MediaStreamTrackSource)

  AudioDestinationTrackSource(MediaStreamAudioDestinationNode* aNode,
                              nsIPrincipal* aPrincipal)
      : MediaStreamTrackSource(aPrincipal, nsString()), mNode(aNode) {}

  void Destroy() override {
    if (mNode) {
      mNode->DestroyMediaStream();
      mNode = nullptr;
    }
  }

  MediaSourceEnum GetMediaSource() const override {
    return MediaSourceEnum::AudioCapture;
  }

  void Stop() override { Destroy(); }

  void Disable() override {}

  void Enable() override {}

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
      mDOMStream(DOMAudioNodeMediaStream::CreateTrackUnionStreamAsInput(
          GetOwner(), this, aContext->Graph())) {
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
  RefPtr<MediaStreamTrackSource> source =
      new AudioDestinationTrackSource(this, principal);
  RefPtr<MediaStreamTrack> track = mDOMStream->CreateDOMTrack(
      AudioNodeStream::AUDIO_TRACK, MediaSegment::AUDIO, source,
      MediaTrackConstraints());
  mDOMStream->AddTrackInternal(track);

  ProcessedMediaStream* outputStream =
      mDOMStream->GetInputStream()->AsProcessedStream();
  MOZ_ASSERT(!!outputStream);
  AudioNodeEngine* engine = new AudioNodeEngine(this);
  mStream = AudioNodeStream::Create(
      aContext, engine, AudioNodeStream::EXTERNAL_OUTPUT, aContext->Graph());
  mPort =
      outputStream->AllocateInputPort(mStream, AudioNodeStream::AUDIO_TRACK);
}

/* static */
already_AddRefed<MediaStreamAudioDestinationNode>
MediaStreamAudioDestinationNode::Create(AudioContext& aAudioContext,
                                        const AudioNodeOptions& aOptions,
                                        ErrorResult& aRv) {
  if (aAudioContext.IsOffline()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

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
  amount += mPort->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t MediaStreamAudioDestinationNode::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void MediaStreamAudioDestinationNode::DestroyMediaStream() {
  AudioNode::DestroyMediaStream();
  if (mPort) {
    mPort->Destroy();
    mPort = nullptr;
  }
}

JSObject* MediaStreamAudioDestinationNode::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MediaStreamAudioDestinationNode_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
