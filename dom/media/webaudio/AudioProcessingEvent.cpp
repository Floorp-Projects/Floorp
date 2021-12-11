/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioProcessingEvent.h"
#include "mozilla/dom/AudioProcessingEventBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "AudioContext.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioProcessingEvent, Event, mInputBuffer,
                                   mOutputBuffer, mNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioProcessingEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(AudioProcessingEvent, Event)
NS_IMPL_RELEASE_INHERITED(AudioProcessingEvent, Event)

AudioProcessingEvent::AudioProcessingEvent(ScriptProcessorNode* aOwner,
                                           nsPresContext* aPresContext,
                                           WidgetEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent), mPlaybackTime(0.0), mNode(aOwner) {}

AudioProcessingEvent::~AudioProcessingEvent() = default;

JSObject* AudioProcessingEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return AudioProcessingEvent_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<AudioBuffer> AudioProcessingEvent::LazilyCreateBuffer(
    uint32_t aNumberOfChannels, ErrorResult& aRv) {
  RefPtr<AudioBuffer> buffer = AudioBuffer::Create(
      mNode->Context()->GetOwner(), aNumberOfChannels, mNode->BufferSize(),
      mNode->Context()->SampleRate(), aRv);
  MOZ_ASSERT(buffer || aRv.ErrorCodeIs(NS_ERROR_OUT_OF_MEMORY));
  return buffer.forget();
}

}  // namespace mozilla::dom
