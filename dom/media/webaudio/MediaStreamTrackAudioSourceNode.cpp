/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamTrackAudioSourceNode.h"
#include "mozilla/dom/MediaStreamTrackAudioSourceNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeExternalInputTrack.h"
#include "AudioStreamTrack.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamTrackAudioSourceNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaStreamTrackAudioSourceNode)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputTrack)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(AudioNode)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    MediaStreamTrackAudioSourceNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputTrack)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamTrackAudioSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(MediaStreamTrackAudioSourceNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(MediaStreamTrackAudioSourceNode, AudioNode)

MediaStreamTrackAudioSourceNode::MediaStreamTrackAudioSourceNode(
    AudioContext* aContext)
    : AudioNode(aContext, 2, ChannelCountMode::Max,
                ChannelInterpretation::Speakers),
      mTrackListener(this) {}

/* static */ already_AddRefed<MediaStreamTrackAudioSourceNode>
MediaStreamTrackAudioSourceNode::Create(
    AudioContext& aAudioContext,
    const MediaStreamTrackAudioSourceOptions& aOptions, ErrorResult& aRv) {
  // The spec has a pointless check here.  See
  // https://github.com/WebAudio/web-audio-api/issues/2149
  MOZ_RELEASE_ASSERT(!aAudioContext.IsOffline(), "Bindings messed up?");

  if (!aOptions.mMediaStreamTrack->Ended() &&
      aAudioContext.Graph() != aOptions.mMediaStreamTrack->Graph()) {
    nsCOMPtr<nsPIDOMWindowInner> pWindow = aAudioContext.GetParentObject();
    Document* document = pWindow ? pWindow->GetExtantDoc() : nullptr;
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "Web Audio"_ns,
                                    document, nsContentUtils::eDOM_PROPERTIES,
                                    "MediaStreamAudioSourceNodeDifferentRate");
    // This is not a spec-required exception, just a limitation of our
    // implementation.
    aRv.ThrowNotSupportedError(
        "Connecting AudioNodes from AudioContexts with different sample-rate "
        "is currently not supported.");
    return nullptr;
  }

  RefPtr<MediaStreamTrackAudioSourceNode> node =
      new MediaStreamTrackAudioSourceNode(&aAudioContext);

  node->Init(aOptions.mMediaStreamTrack, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return node.forget();
}

void MediaStreamTrackAudioSourceNode::Init(MediaStreamTrack* aMediaStreamTrack,
                                           ErrorResult& aRv) {
  MOZ_ASSERT(aMediaStreamTrack);

  if (!aMediaStreamTrack->AsAudioStreamTrack()) {
    aRv.ThrowInvalidStateError("\"mediaStreamTrack\" must be an audio track");
    return;
  }

  if (aMediaStreamTrack->Ended()) {
    // The track is ended and will never produce any data. Pretend like this is
    // fine.
    return;
  }

  MarkActive();

  MediaTrackGraph* graph = Context()->Graph();

  AudioNodeEngine* engine = new MediaStreamTrackAudioSourceNodeEngine(this);
  mTrack = AudioNodeExternalInputTrack::Create(graph, engine);

  MOZ_ASSERT(mTrack);

  mInputTrack = aMediaStreamTrack;
  ProcessedMediaTrack* outputTrack =
      static_cast<ProcessedMediaTrack*>(mTrack.get());
  mInputPort = mInputTrack->ForwardTrackContentsTo(outputTrack);
  PrincipalChanged(mInputTrack);  // trigger enabling/disabling of the connector
  mInputTrack->AddPrincipalChangeObserver(this);

  mInputTrack->AddConsumer(&mTrackListener);
}

void MediaStreamTrackAudioSourceNode::Destroy() {
  if (mInputTrack) {
    mTrackListener.NotifyEnded(mInputTrack);
    mInputTrack->RemovePrincipalChangeObserver(this);
    mInputTrack->RemoveConsumer(&mTrackListener);
    mInputTrack = nullptr;
  }

  if (mInputPort) {
    mInputPort->Destroy();
    mInputPort = nullptr;
  }
}

MediaStreamTrackAudioSourceNode::~MediaStreamTrackAudioSourceNode() {
  Destroy();
}

/**
 * Changes the principal. Note that this will be called on the main thread, but
 * changes will be enacted on the MediaTrackGraph thread. If the principal
 * change results in the document principal losing access to the track, then
 * there needs to be other measures in place to ensure that any media that is
 * governed by the new track principal is not available to the MediaTrackGraph
 * before this change completes. Otherwise, a site could get access to
 * media that they are not authorized to receive.
 *
 * One solution is to block the altered content, call this method, then dispatch
 * another change request to the MediaTrackGraph thread that allows the content
 * under the new principal to flow. This might be unnecessary if the principal
 * change is changing to be the document principal.
 */
void MediaStreamTrackAudioSourceNode::PrincipalChanged(
    MediaStreamTrack* aMediaStreamTrack) {
  MOZ_ASSERT(aMediaStreamTrack == mInputTrack);

  bool subsumes = false;
  Document* doc = nullptr;
  if (nsPIDOMWindowInner* parent = Context()->GetParentObject()) {
    doc = parent->GetExtantDoc();
    if (doc) {
      nsIPrincipal* docPrincipal = doc->NodePrincipal();
      nsIPrincipal* trackPrincipal = aMediaStreamTrack->GetPrincipal();
      if (!trackPrincipal ||
          NS_FAILED(docPrincipal->Subsumes(trackPrincipal, &subsumes))) {
        subsumes = false;
      }
    }
  }
  auto track = static_cast<AudioNodeExternalInputTrack*>(mTrack.get());
  bool enabled = subsumes;
  track->SetInt32Parameter(MediaStreamTrackAudioSourceNodeEngine::ENABLE,
                           enabled);
  fprintf(stderr, "NOW: %s", enabled ? "enabled" : "disabled");

  if (!enabled && doc) {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "Web Audio"_ns,
                                    doc, nsContentUtils::eDOM_PROPERTIES,
                                    CrossOriginErrorString());
  }
}

size_t MediaStreamTrackAudioSourceNode::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  if (mInputPort) {
    amount += mInputPort->SizeOfIncludingThis(aMallocSizeOf);
  }
  return amount;
}

size_t MediaStreamTrackAudioSourceNode::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void MediaStreamTrackAudioSourceNode::DestroyMediaTrack() {
  if (mInputPort) {
    mInputPort->Destroy();
    mInputPort = nullptr;
  }
  AudioNode::DestroyMediaTrack();
}

JSObject* MediaStreamTrackAudioSourceNode::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MediaStreamTrackAudioSourceNode_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
