/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioProcessingEvent.h"
#include "mozilla/dom/AudioProcessingEventBinding.h"
#include "AudioContext.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_3(AudioProcessingEvent, Event,
                                     mInputBuffer, mOutputBuffer, mNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioProcessingEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(AudioProcessingEvent, Event)
NS_IMPL_RELEASE_INHERITED(AudioProcessingEvent, Event)

AudioProcessingEvent::AudioProcessingEvent(ScriptProcessorNode* aOwner,
                                           nsPresContext* aPresContext,
                                           WidgetEvent* aEvent)
  : Event(aOwner, aPresContext, aEvent)
  , mPlaybackTime(0.0)
  , mNode(aOwner)
{
  SetIsDOMBinding();
}

JSObject*
AudioProcessingEvent::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return AudioProcessingEventBinding::Wrap(aCx, aScope, this);
}

void
AudioProcessingEvent::LazilyCreateBuffer(nsRefPtr<AudioBuffer>& aBuffer,
                                         uint32_t aNumberOfChannels)
{
  AutoPushJSContext cx(mNode->Context()->GetJSContext());

  aBuffer = new AudioBuffer(mNode->Context(), mNode->BufferSize(),
                            mNode->Context()->SampleRate());
  aBuffer->InitializeBuffers(aNumberOfChannels, cx);
}

}
}

