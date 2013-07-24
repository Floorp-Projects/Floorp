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

MediaStreamAudioSourceNode::MediaStreamAudioSourceNode(AudioContext* aContext,
                                                       const DOMMediaStream* aMediaStream)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
{
  AudioNodeEngine* engine = new AudioNodeEngine(this);
  mStream = aContext->Graph()->CreateAudioNodeExternalInputStream(engine);
  ProcessedMediaStream* outputStream = static_cast<ProcessedMediaStream*>(mStream.get());
  mInputPort = outputStream->AllocateInputPort(aMediaStream->GetStream(),
                                               MediaInputPort::FLAG_BLOCK_INPUT);
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
MediaStreamAudioSourceNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MediaStreamAudioSourceNodeBinding::Wrap(aCx, aScope, this);
}

}
}

