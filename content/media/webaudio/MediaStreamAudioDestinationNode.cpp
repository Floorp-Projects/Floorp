/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamAudioDestinationNode.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/MediaStreamAudioDestinationNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "DOMMediaStream.h"
#include "TrackUnionStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(MediaStreamAudioDestinationNode, AudioNode, mDOMStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaStreamAudioDestinationNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(MediaStreamAudioDestinationNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(MediaStreamAudioDestinationNode, AudioNode)

// This must be a different value than AUDIO_NODE_STREAM_TRACK_ID
static const int MEDIA_STREAM_DEST_TRACK_ID = 2;

class MediaStreamDestinationEngine : public AudioNodeEngine {
public:
  MediaStreamDestinationEngine(AudioNode* aNode, ProcessedMediaStream* aOutputStream)
    : AudioNodeEngine(aNode)
    , mOutputStream(aOutputStream)
  {
    MOZ_ASSERT(mOutputStream);
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished) MOZ_OVERRIDE
  {
    *aOutput = aInput;
    StreamBuffer::Track* track = mOutputStream->EnsureTrack(MEDIA_STREAM_DEST_TRACK_ID,
                                                            aStream->SampleRate());
    AudioSegment* segment = track->Get<AudioSegment>();
    segment->AppendAndConsumeChunk(aOutput);
  }

private:
  ProcessedMediaStream* mOutputStream;
};

// This callback is used to ensure that only the audio data for this track is audible
static bool FilterAudioNodeStreamTrack(StreamBuffer::Track* aTrack)
{
  return aTrack->GetID() == MEDIA_STREAM_DEST_TRACK_ID;
}

MediaStreamAudioDestinationNode::MediaStreamAudioDestinationNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Explicit,
              ChannelInterpretation::Speakers)
  , mDOMStream(DOMAudioNodeMediaStream::CreateTrackUnionStream(GetOwner(),
                                                               MOZ_THIS_IN_INITIALIZER_LIST(),
                                                               DOMMediaStream::HINT_CONTENTS_AUDIO))
{
  TrackUnionStream* tus = static_cast<TrackUnionStream*>(mDOMStream->GetStream());
  MOZ_ASSERT(tus == mDOMStream->GetStream()->AsProcessedStream());
  tus->SetTrackIDFilter(FilterAudioNodeStreamTrack);

  MediaStreamDestinationEngine* engine = new MediaStreamDestinationEngine(this, tus);
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);
  mPort = tus->AllocateInputPort(mStream, 0);
}

void
MediaStreamAudioDestinationNode::DestroyMediaStream()
{
  AudioNode::DestroyMediaStream();
  if (mPort) {
    mPort->Destroy();
    mPort = nullptr;
  }
}

JSObject*
MediaStreamAudioDestinationNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MediaStreamAudioDestinationNodeBinding::Wrap(aCx, aScope, this);
}

}
}
