/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMNotifyAudioAvailableEvent_h_
#define nsDOMNotifyAudioAvailableEvent_h_

#include "nsIDOMNotifyAudioAvailableEvent.h"
#include "nsDOMEvent.h"
#include "nsPresContext.h"
#include "nsCycleCollectionParticipant.h"

class nsDOMNotifyAudioAvailableEvent : public nsDOMEvent,
                                       public nsIDOMNotifyAudioAvailableEvent
{
public:
  nsDOMNotifyAudioAvailableEvent(nsPresContext* aPresContext, nsEvent* aEvent,
                                 PRUint32 aEventType, float * aFrameBuffer,
                                 PRUint32 aFrameBufferLength, float aTime);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsDOMNotifyAudioAvailableEvent,
                                                         nsDOMEvent)

  NS_DECL_NSIDOMNOTIFYAUDIOAVAILABLEEVENT
  NS_FORWARD_NSIDOMEVENT(nsDOMEvent::)

  nsresult NS_NewDOMAudioAvailableEvent(nsIDOMEvent** aInstancePtrResult,
                                        nsPresContext* aPresContext,
                                        nsEvent *aEvent,
                                        PRUint32 aEventType,
                                        float * aFrameBuffer,
                                        PRUint32 aFrameBufferLength,
                                        float aTime);

  ~nsDOMNotifyAudioAvailableEvent();

private:
  nsAutoArrayPtr<float> mFrameBuffer;
  PRUint32 mFrameBufferLength;
  float mTime;
  JSObject* mCachedArray;
  bool mAllowAudioData;
};

#endif // nsDOMNotifyAudioAvailableEvent_h_
