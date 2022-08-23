/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamAudioSourceNode.h"
#include "mozilla/dom/MediaStreamAudioSourceNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeExternalInputTrack.h"
#include "AudioStreamTrack.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsID.h"
#include "Tracing.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamAudioSourceNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaStreamAudioSourceNode)
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputTrack)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(AudioNode)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaStreamAudioSourceNode,
                                                  AudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputTrack)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaStreamAudioSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(MediaStreamAudioSourceNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(MediaStreamAudioSourceNode, AudioNode)

MediaStreamAudioSourceNode::MediaStreamAudioSourceNode(
    AudioContext* aContext, TrackChangeBehavior aBehavior)
    : AudioNode(aContext, 2, ChannelCountMode::Max,
                ChannelInterpretation::Speakers),
      mBehavior(aBehavior) {}

/* static */
already_AddRefed<MediaStreamAudioSourceNode> MediaStreamAudioSourceNode::Create(
    AudioContext& aAudioContext, const MediaStreamAudioSourceOptions& aOptions,
    ErrorResult& aRv) {
  // The spec has a pointless check here.  See
  // https://github.com/WebAudio/web-audio-api/issues/2149
  MOZ_RELEASE_ASSERT(!aAudioContext.IsOffline(), "Bindings messed up?");

  RefPtr<MediaStreamAudioSourceNode> node =
      new MediaStreamAudioSourceNode(&aAudioContext, LockOnTrackPicked);

  // aOptions.mMediaStream is not nullable.
  node->Init(*aOptions.mMediaStream, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return node.forget();
}

void MediaStreamAudioSourceNode::Init(DOMMediaStream& aMediaStream,
                                      ErrorResult& aRv) {
  mInputStream = &aMediaStream;
  AudioNodeEngine* engine = new MediaStreamAudioSourceNodeEngine(this);
  mTrack = AudioNodeExternalInputTrack::Create(Context()->Graph(), engine);
  mInputStream->AddConsumerToKeepAlive(ToSupports(this));

  mInputStream->RegisterTrackListener(this);
  if (mInputStream->Audible()) {
    NotifyAudible();
  }
  AttachToRightTrack(mInputStream, aRv);
}

void MediaStreamAudioSourceNode::Destroy() {
  if (mInputStream) {
    mInputStream->UnregisterTrackListener(this);
    mInputStream = nullptr;
  }
  DetachFromTrack();
}

MediaStreamAudioSourceNode::~MediaStreamAudioSourceNode() { Destroy(); }

void MediaStreamAudioSourceNode::AttachToTrack(
    const RefPtr<MediaStreamTrack>& aTrack, ErrorResult& aRv) {
  MOZ_ASSERT(!mInputTrack);
  MOZ_ASSERT(aTrack->AsAudioStreamTrack());
  MOZ_DIAGNOSTIC_ASSERT(!aTrack->Ended());

  if (!mTrack) {
    return;
  }

  if (NS_WARN_IF(Context()->Graph() != aTrack->Graph())) {
    nsCOMPtr<nsPIDOMWindowInner> pWindow = Context()->GetParentObject();
    Document* document = pWindow ? pWindow->GetExtantDoc() : nullptr;
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "Web Audio"_ns,
                                    document, nsContentUtils::eDOM_PROPERTIES,
                                    "MediaStreamAudioSourceNodeDifferentRate");
    // This is not a spec-required exception, just a limitation of our
    // implementation.
    aRv.ThrowNotSupportedError(
        "Connecting AudioNodes from AudioContexts with different sample-rate "
        "is currently not supported.");
    return;
  }

  mInputTrack = aTrack;
  ProcessedMediaTrack* outputTrack =
      static_cast<ProcessedMediaTrack*>(mTrack.get());
  mInputPort = mInputTrack->ForwardTrackContentsTo(outputTrack);
  PrincipalChanged(mInputTrack);  // trigger enabling/disabling of the connector
  mInputTrack->AddPrincipalChangeObserver(this);
  MarkActive();
}

void MediaStreamAudioSourceNode::DetachFromTrack() {
  if (mInputTrack) {
    mInputTrack->RemovePrincipalChangeObserver(this);
    mInputTrack = nullptr;
  }
  if (mInputPort) {
    mInputPort->Destroy();
    mInputPort = nullptr;
  }
}

static int AudioTrackCompare(const RefPtr<AudioStreamTrack>& aLhs,
                             const RefPtr<AudioStreamTrack>& aRhs) {
  nsAutoStringN<NSID_LENGTH> IDLhs;
  nsAutoStringN<NSID_LENGTH> IDRhs;
  aLhs->GetId(IDLhs);
  aRhs->GetId(IDRhs);
  return Compare(NS_ConvertUTF16toUTF8(IDLhs), NS_ConvertUTF16toUTF8(IDRhs));
}

void MediaStreamAudioSourceNode::AttachToRightTrack(
    const RefPtr<DOMMediaStream>& aMediaStream, ErrorResult& aRv) {
  nsTArray<RefPtr<AudioStreamTrack>> tracks;
  aMediaStream->GetAudioTracks(tracks);

  if (tracks.IsEmpty() && mBehavior == LockOnTrackPicked) {
    aRv.ThrowInvalidStateError("No audio tracks in MediaStream");
    return;
  }

  // Sort the track to have a stable order, on their ID by lexicographic
  // ordering on sequences of code unit values.
  tracks.Sort(AudioTrackCompare);

  for (const RefPtr<AudioStreamTrack>& track : tracks) {
    if (mBehavior == FollowChanges) {
      if (track->Ended()) {
        continue;
      }
    }

    if (!track->Ended()) {
      AttachToTrack(track, aRv);
    }
    return;
  }

  // There was no track available. We'll allow the node to be garbage collected.
  MarkInactive();
}

void MediaStreamAudioSourceNode::NotifyTrackAdded(
    const RefPtr<MediaStreamTrack>& aTrack) {
  if (mBehavior != FollowChanges) {
    return;
  }
  if (mInputTrack) {
    return;
  }

  if (!aTrack->AsAudioStreamTrack()) {
    return;
  }

  AttachToTrack(aTrack, IgnoreErrors());
}

void MediaStreamAudioSourceNode::NotifyTrackRemoved(
    const RefPtr<MediaStreamTrack>& aTrack) {
  if (mBehavior == FollowChanges) {
    if (aTrack != mInputTrack) {
      return;
    }

    DetachFromTrack();
    AttachToRightTrack(mInputStream, IgnoreErrors());
  }
}

void MediaStreamAudioSourceNode::NotifyAudible() {
  MOZ_ASSERT(mInputStream);
  Context()->StartBlockedAudioContextIfAllowed();
}

/**
 * Changes the principal. Note that this will be called on the main thread, but
 * changes will be enacted on the MediaTrackGraph thread. If the principal
 * change results in the document principal losing access to the stream, then
 * there needs to be other measures in place to ensure that any media that is
 * governed by the new stream principal is not available to the MediaTrackGraph
 * before this change completes. Otherwise, a site could get access to
 * media that they are not authorized to receive.
 *
 * One solution is to block the altered content, call this method, then dispatch
 * another change request to the MediaTrackGraph thread that allows the content
 * under the new principal to flow. This might be unnecessary if the principal
 * change is changing to be the document principal.
 */
void MediaStreamAudioSourceNode::PrincipalChanged(
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
  track->SetInt32Parameter(MediaStreamAudioSourceNodeEngine::ENABLE, enabled);

  if (!enabled && doc) {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "Web Audio"_ns,
                                    doc, nsContentUtils::eDOM_PROPERTIES,
                                    CrossOriginErrorString());
  }
}

size_t MediaStreamAudioSourceNode::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  // Future:
  // - mInputStream
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  if (mInputPort) {
    amount += mInputPort->SizeOfIncludingThis(aMallocSizeOf);
  }
  return amount;
}

size_t MediaStreamAudioSourceNode::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void MediaStreamAudioSourceNode::DestroyMediaTrack() {
  if (mInputPort) {
    mInputPort->Destroy();
    mInputPort = nullptr;
  }
  AudioNode::DestroyMediaTrack();
}

JSObject* MediaStreamAudioSourceNode::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MediaStreamAudioSourceNode_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
