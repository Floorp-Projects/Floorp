/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamAudioSourceNode.h"
#include "mozilla/dom/MediaStreamAudioSourceNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeExternalInputStream.h"
#include "DOMMediaStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaStreamAudioSourceNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(MediaStreamAudioSourceNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputStream)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(AudioNode)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaStreamAudioSourceNode, AudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputStream)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaStreamAudioSourceNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(MediaStreamAudioSourceNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(MediaStreamAudioSourceNode, AudioNode)

MediaStreamAudioSourceNode::MediaStreamAudioSourceNode(AudioContext* aContext,
                                                       DOMMediaStream* aMediaStream)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers),
    mInputStream(aMediaStream)
{
  AudioNodeEngine* engine = new AudioNodeEngine(this);
  mStream = aContext->Graph()->CreateAudioNodeExternalInputStream(engine);
  ProcessedMediaStream* outputStream = static_cast<ProcessedMediaStream*>(mStream.get());
  mInputPort = outputStream->AllocateInputPort(aMediaStream->GetStream(),
                                               MediaInputPort::FLAG_BLOCK_INPUT);
  mInputStream->AddConsumerToKeepAlive(this);
}

MediaStreamAudioSourceNode::~MediaStreamAudioSourceNode()
{
}

void
MediaStreamAudioSourceNode::DestroyMediaStream()
{
  if (mInputPort) {
    mInputPort->Destroy();
    mInputPort = nullptr;
  }
  AudioNode::DestroyMediaStream();
}

JSObject*
MediaStreamAudioSourceNode::WrapObject(JSContext* aCx)
{
  return MediaStreamAudioSourceNodeBinding::Wrap(aCx, this);
}

}
}

