/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioProcessingEvent_h_
#define AudioProcessingEvent_h_

#include "nsDOMEvent.h"
#include "AudioBuffer.h"
#include "ScriptProcessorNode.h"

namespace mozilla {
namespace dom {

class AudioProcessingEvent : public nsDOMEvent,
                             public EnableWebAudioCheck
{
public:
  AudioProcessingEvent(ScriptProcessorNode* aOwner,
                       nsPresContext *aPresContext,
                       nsEvent *aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioProcessingEvent, nsDOMEvent)

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope);

  void InitEvent(AudioBuffer* aInputBuffer,
                 uint32_t aNumberOfInputChannels,
                 double aPlaybackTime)
  {
    InitEvent(NS_LITERAL_STRING("audioprocess"), false, false);
    mInputBuffer = aInputBuffer;
    mNumberOfInputChannels = aNumberOfInputChannels;
    mPlaybackTime = aPlaybackTime;
  }

  double PlaybackTime() const
  {
    return mPlaybackTime;
  }

  AudioBuffer* InputBuffer()
  {
    if (!mInputBuffer) {
      LazilyCreateBuffer(mInputBuffer, mNumberOfInputChannels);
    }
    return mInputBuffer;
  }

  AudioBuffer* OutputBuffer()
  {
    if (!mOutputBuffer) {
      LazilyCreateBuffer(mOutputBuffer, mNode->NumberOfOutputChannels());
    }
    return mOutputBuffer;
  }

  bool HasOutputBuffer() const
  {
    return !!mOutputBuffer;
  }

private:
  void LazilyCreateBuffer(nsRefPtr<AudioBuffer>& aBuffer,
                          uint32_t aNumberOfChannels);

private:
  double mPlaybackTime;
  nsRefPtr<AudioBuffer> mInputBuffer;
  nsRefPtr<AudioBuffer> mOutputBuffer;
  nsRefPtr<ScriptProcessorNode> mNode;
  uint32_t mNumberOfInputChannels;
};

}
}

#endif

