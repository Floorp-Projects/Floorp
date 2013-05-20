/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsDOMNotifyAudioAvailableEvent.h"
#include "nsContentUtils.h" // NS_DROP_JS_OBJECTS
#include "jsfriendapi.h"

using namespace mozilla;
using namespace mozilla::dom;

nsDOMNotifyAudioAvailableEvent::nsDOMNotifyAudioAvailableEvent(EventTarget* aOwner,
                                                               nsPresContext* aPresContext,
                                                               nsEvent* aEvent,
                                                               uint32_t aEventType,
                                                               float* aFrameBuffer,
                                                               uint32_t aFrameBufferLength,
                                                               float aTime)
  : nsDOMEvent(aOwner, aPresContext, aEvent),
    mFrameBuffer(aFrameBuffer),
    mFrameBufferLength(aFrameBufferLength),
    mTime(aTime),
    mCachedArray(nullptr),
    mAllowAudioData(false)
{
  MOZ_COUNT_CTOR(nsDOMNotifyAudioAvailableEvent);
  if (mEvent) {
    mEvent->message = aEventType;
  }
  SetIsDOMBinding();
}

NS_IMPL_ADDREF_INHERITED(nsDOMNotifyAudioAvailableEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMNotifyAudioAvailableEvent, nsDOMEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMNotifyAudioAvailableEvent, nsDOMEvent)
  if (tmp->mCachedArray) {
    tmp->mCachedArray = nullptr;
    NS_DROP_JS_OBJECTS(tmp, nsDOMNotifyAudioAvailableEvent);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMNotifyAudioAvailableEvent, nsDOMEvent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsDOMNotifyAudioAvailableEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCachedArray)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMNotifyAudioAvailableEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNotifyAudioAvailableEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

nsDOMNotifyAudioAvailableEvent::~nsDOMNotifyAudioAvailableEvent()
{
  MOZ_COUNT_DTOR(nsDOMNotifyAudioAvailableEvent);
  if (mCachedArray) {
    mCachedArray = nullptr;
    NS_DROP_JS_OBJECTS(this, nsDOMNotifyAudioAvailableEvent);
  }
}

NS_IMETHODIMP
nsDOMNotifyAudioAvailableEvent::GetFrameBuffer(JSContext* aCx, JS::Value* aResult)
{
  if (!mAllowAudioData) {
    // Media is not same-origin, don't allow the data out.
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (mCachedArray) {
    *aResult = OBJECT_TO_JSVAL(mCachedArray);
    return NS_OK;
  }

  // Cache this array so we don't recreate on next call.
  NS_HOLD_JS_OBJECTS(this, nsDOMNotifyAudioAvailableEvent);

  mCachedArray = JS_NewFloat32Array(aCx, mFrameBufferLength);
  if (!mCachedArray) {
    NS_DROP_JS_OBJECTS(this, nsDOMNotifyAudioAvailableEvent);
    return NS_ERROR_FAILURE;
  }
  memcpy(JS_GetFloat32ArrayData(mCachedArray), mFrameBuffer.get(), mFrameBufferLength * sizeof(float));

  *aResult = OBJECT_TO_JSVAL(mCachedArray);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMNotifyAudioAvailableEvent::GetTime(float *aRetVal)
{
  *aRetVal = Time();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMNotifyAudioAvailableEvent::InitAudioAvailableEvent(const nsAString& aType,
                                                        bool aCanBubble,
                                                        bool aCancelable,
                                                        float* aFrameBuffer,
                                                        uint32_t aFrameBufferLength,
                                                        float aTime,
                                                        bool aAllowAudioData)
{
  // Auto manage the memory which stores the frame buffer. This ensures
  // that if we exit due to some error, the memory will be freed. Otherwise,
  // the framebuffer's memory will be freed when this event is destroyed.
  nsAutoArrayPtr<float> frameBuffer(aFrameBuffer);
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mFrameBuffer = frameBuffer.forget();
  mFrameBufferLength = aFrameBufferLength;
  mTime = aTime;
  mAllowAudioData = aAllowAudioData;
  mCachedArray = nullptr;
  return NS_OK;
}

void
nsDOMNotifyAudioAvailableEvent::InitAudioAvailableEvent(const nsAString& aType,
                                                        bool aCanBubble,
                                                        bool aCancelable,
                                                        const Nullable<Sequence<float> >& aFrameBuffer,
                                                        uint32_t aFrameBufferLength,
                                                        float aTime,
                                                        bool aAllowAudioData,
                                                        ErrorResult& aRv)
{
  if ((aFrameBuffer.IsNull() && aFrameBufferLength > 0) ||
      (!aFrameBuffer.IsNull() &&
       aFrameBuffer.Value().Length() < aFrameBufferLength)) {
    aRv = NS_ERROR_UNEXPECTED;
    return;
  }

  nsAutoArrayPtr<float> buffer;
  if (!aFrameBuffer.IsNull()) {
    buffer = new float[aFrameBufferLength];
    memcpy(buffer.get(), aFrameBuffer.Value().Elements(),
           aFrameBufferLength * sizeof(float));
  }

  aRv = InitAudioAvailableEvent(aType, aCanBubble, aCancelable,
                                buffer.forget(),
                                aFrameBufferLength,
                                aTime, aAllowAudioData);
}

nsresult NS_NewDOMAudioAvailableEvent(nsIDOMEvent** aInstancePtrResult,
                                      EventTarget* aOwner,
                                      nsPresContext* aPresContext,
                                      nsEvent *aEvent,
                                      uint32_t aEventType,
                                      float* aFrameBuffer,
                                      uint32_t aFrameBufferLength,
                                      float aTime)
{
  nsDOMNotifyAudioAvailableEvent* it =
    new nsDOMNotifyAudioAvailableEvent(aOwner, aPresContext, aEvent, aEventType,
                                       aFrameBuffer, aFrameBufferLength, aTime);
  return CallQueryInterface(it, aInstancePtrResult);
}
