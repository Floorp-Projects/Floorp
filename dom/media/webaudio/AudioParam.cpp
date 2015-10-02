/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioParam.h"
#include "mozilla/dom/AudioParamBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioContext.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioParam)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioParam)
  tmp->DisconnectFromGraphAndDestroyStream();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AudioParam)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(AudioParam)

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(AudioParam)

NS_IMETHODIMP_(MozExternalRefCountType)
AudioParam::Release()
{
  if (mRefCnt.get() == 1) {
    // We are about to be deleted, disconnect the object from the graph before
    // the derived type is destroyed.
    DisconnectFromGraphAndDestroyStream();
  }
  NS_IMPL_CC_NATIVE_RELEASE_BODY(AudioParam)
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AudioParam, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AudioParam, Release)

AudioParam::AudioParam(AudioNode* aNode,
                       uint32_t aIndex,
                       float aDefaultValue,
                       const char* aName)
  : AudioParamTimeline(aDefaultValue)
  , mNode(aNode)
  , mName(aName)
  , mIndex(aIndex)
  , mDefaultValue(aDefaultValue)
{
}

AudioParam::~AudioParam()
{
  MOZ_ASSERT(mInputNodes.IsEmpty());
}

JSObject*
AudioParam::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AudioParamBinding::Wrap(aCx, this, aGivenProto);
}

void
AudioParam::DisconnectFromGraphAndDestroyStream()
{
  // Addref this temporarily so the refcount bumping below doesn't destroy us
  // prematurely
  nsRefPtr<AudioParam> kungFuDeathGrip = this;

  while (!mInputNodes.IsEmpty()) {
    uint32_t i = mInputNodes.Length() - 1;
    nsRefPtr<AudioNode> input = mInputNodes[i].mInputNode;
    mInputNodes.RemoveElementAt(i);
    input->RemoveOutputParam(this);
  }

  if (mNodeStreamPort) {
    mNodeStreamPort->Destroy();
    mNodeStreamPort = nullptr;
  }

  if (mStream) {
    mStream->Destroy();
    mStream = nullptr;
  }
}

MediaStream*
AudioParam::Stream()
{
  if (mStream) {
    return mStream;
  }

  AudioNodeEngine* engine = new AudioNodeEngine(nullptr);
  nsRefPtr<AudioNodeStream> stream =
    AudioNodeStream::Create(mNode->Context(), engine,
                            AudioNodeStream::NO_STREAM_FLAGS);

  // Force the input to have only one channel, and make it down-mix using
  // the speaker rules if needed.
  stream->SetChannelMixingParametersImpl(1, ChannelCountMode::Explicit, ChannelInterpretation::Speakers);
  // Mark as an AudioParam helper stream
  stream->SetAudioParamHelperStream();

  mStream = stream.forget();

  // Setup the AudioParam's stream as an input to the owner AudioNode's stream
  AudioNodeStream* nodeStream = mNode->GetStream();
  if (nodeStream) {
    mNodeStreamPort =
      nodeStream->AllocateInputPort(mStream, AudioNodeStream::AUDIO_TRACK);
  }

  // Send the stream to the timeline on the MSG side.
  AudioTimelineEvent event(mStream);
  SendEventToEngine(event);

  return mStream;
}

void
AudioParam::SendEventToEngine(const AudioTimelineEvent& aEvent)
{
  AudioNodeStream* stream = mNode->GetStream();
  if (stream) {
    stream->SendTimelineEvent(mIndex, aEvent);
  }
}

float
AudioParamTimeline::AudioNodeInputValue(size_t aCounter) const
{
  MOZ_ASSERT(mStream);

  // If we have a chunk produced by the AudioNode inputs to the AudioParam,
  // get its value now.  We use aCounter to tell us which frame of the last
  // AudioChunk to look at.
  float audioNodeInputValue = 0.0f;
  const AudioBlock& lastAudioNodeChunk =
    static_cast<AudioNodeStream*>(mStream.get())->LastChunks()[0];
  if (!lastAudioNodeChunk.IsNull()) {
    MOZ_ASSERT(lastAudioNodeChunk.GetDuration() == WEBAUDIO_BLOCK_SIZE);
    audioNodeInputValue =
      static_cast<const float*>(lastAudioNodeChunk.mChannelData[0])[aCounter];
    audioNodeInputValue *= lastAudioNodeChunk.mVolume;
  }

  return audioNodeInputValue;
}

} // namespace dom
} // namespace mozilla

