/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OfflineAudioCompletionEvent.h"
#include "mozilla/dom/OfflineAudioCompletionEventBinding.h"
#include "AudioContext.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(OfflineAudioCompletionEvent, Event,
                                     mRenderedBuffer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(OfflineAudioCompletionEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(OfflineAudioCompletionEvent, Event)
NS_IMPL_RELEASE_INHERITED(OfflineAudioCompletionEvent, Event)

OfflineAudioCompletionEvent::OfflineAudioCompletionEvent(AudioContext* aOwner,
                                                         nsPresContext* aPresContext,
                                                         WidgetEvent* aEvent)
  : Event(aOwner, aPresContext, aEvent)
{
  SetIsDOMBinding();
}

JSObject*
OfflineAudioCompletionEvent::WrapObject(JSContext* aCx)
{
  return OfflineAudioCompletionEventBinding::Wrap(aCx, this);
}

}
}

