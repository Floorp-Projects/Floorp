/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioProcessingEvent_h_
#define AudioProcessingEvent_h_

#include "AudioBuffer.h"
#include "ScriptProcessorNode.h"
#include "mozilla/dom/Event.h"

namespace mozilla {
namespace dom {

class AudioProcessingEvent final : public Event {
 public:
  AudioProcessingEvent(ScriptProcessorNode* aOwner, nsPresContext* aPresContext,
                       WidgetEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioProcessingEvent, Event)

  JSObject* WrapObjectInternal(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  using Event::InitEvent;
  void InitEvent(AudioBuffer* aInputBuffer, uint32_t aNumberOfInputChannels,
                 double aPlaybackTime) {
    InitEvent(u"audioprocess"_ns, CanBubble::eNo, Cancelable::eNo);
    mInputBuffer = aInputBuffer;
    mNumberOfInputChannels = aNumberOfInputChannels;
    mPlaybackTime = aPlaybackTime;
  }

  double PlaybackTime() const { return mPlaybackTime; }

  AudioBuffer* GetInputBuffer(ErrorResult& aRv) {
    if (!mInputBuffer) {
      mInputBuffer = LazilyCreateBuffer(mNumberOfInputChannels, aRv);
    }
    return mInputBuffer;
  }

  AudioBuffer* GetOutputBuffer(ErrorResult& aRv) {
    if (!mOutputBuffer) {
      mOutputBuffer = LazilyCreateBuffer(mNode->NumberOfOutputChannels(), aRv);
    }
    return mOutputBuffer;
  }

  bool HasOutputBuffer() const { return !!mOutputBuffer; }

 protected:
  virtual ~AudioProcessingEvent();

 private:
  already_AddRefed<AudioBuffer> LazilyCreateBuffer(uint32_t aNumberOfChannels,
                                                   ErrorResult& rv);

 private:
  double mPlaybackTime;
  RefPtr<AudioBuffer> mInputBuffer;
  RefPtr<AudioBuffer> mOutputBuffer;
  RefPtr<ScriptProcessorNode> mNode;
  uint32_t mNumberOfInputChannels;
};

}  // namespace dom
}  // namespace mozilla

#endif
