/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OfflineAudioCompletionEvent_h_
#define OfflineAudioCompletionEvent_h_

#include "AudioBuffer.h"
#include "mozilla/dom/Event.h"

namespace mozilla {
namespace dom {

class AudioContext;

class OfflineAudioCompletionEvent final : public Event
{
public:
  OfflineAudioCompletionEvent(AudioContext* aOwner,
                              nsPresContext* aPresContext,
                              WidgetEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_EVENT
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(OfflineAudioCompletionEvent, Event)

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void InitEvent(AudioBuffer* aRenderedBuffer)
  {
    InitEvent(NS_LITERAL_STRING("complete"), false, false);
    mRenderedBuffer = aRenderedBuffer;
  }

  AudioBuffer* RenderedBuffer() const
  {
    return mRenderedBuffer;
  }

protected:
  virtual ~OfflineAudioCompletionEvent();

private:
  nsRefPtr<AudioBuffer> mRenderedBuffer;
};

}
}

#endif

