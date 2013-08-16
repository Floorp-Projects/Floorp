/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OfflineAudioCompletionEvent_h_
#define OfflineAudioCompletionEvent_h_

#include "nsDOMEvent.h"
#include "AudioBuffer.h"

namespace mozilla {
namespace dom {

class AudioContext;

class OfflineAudioCompletionEvent : public nsDOMEvent,
                                    public EnableWebAudioCheck
{
public:
  OfflineAudioCompletionEvent(AudioContext* aOwner,
                              nsPresContext *aPresContext,
                              nsEvent *aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(OfflineAudioCompletionEvent, nsDOMEvent)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  void InitEvent(AudioBuffer* aRenderedBuffer)
  {
    InitEvent(NS_LITERAL_STRING("complete"), false, false);
    mRenderedBuffer = aRenderedBuffer;
  }

  AudioBuffer* RenderedBuffer() const
  {
    return mRenderedBuffer;
  }

private:
  nsRefPtr<AudioBuffer> mRenderedBuffer;
};

}
}

#endif

