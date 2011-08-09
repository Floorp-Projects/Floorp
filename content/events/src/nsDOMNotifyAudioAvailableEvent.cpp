/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Humphrey <david.humphrey@senecac.on.ca>
 *  Yury Delendik
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMNotifyAudioAvailableEvent.h"
#include "jstypedarray.h"

nsDOMNotifyAudioAvailableEvent::nsDOMNotifyAudioAvailableEvent(nsPresContext* aPresContext,
                                                               nsEvent* aEvent,
                                                               PRUint32 aEventType,
                                                               float* aFrameBuffer,
                                                               PRUint32 aFrameBufferLength,
                                                               float aTime)
  : nsDOMEvent(aPresContext, aEvent),
    mFrameBuffer(aFrameBuffer),
    mFrameBufferLength(aFrameBufferLength),
    mTime(aTime),
    mCachedArray(nsnull),
    mAllowAudioData(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsDOMNotifyAudioAvailableEvent);
  if (mEvent) {
    mEvent->message = aEventType;
  }
}

DOMCI_DATA(NotifyAudioAvailableEvent, nsDOMNotifyAudioAvailableEvent)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMNotifyAudioAvailableEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMNotifyAudioAvailableEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMNotifyAudioAvailableEvent, nsDOMEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMNotifyAudioAvailableEvent, nsDOMEvent)
  if (tmp->mCachedArray) {
    NS_DROP_JS_OBJECTS(tmp, nsDOMNotifyAudioAvailableEvent);
    tmp->mCachedArray = nsnull;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMNotifyAudioAvailableEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMNotifyAudioAvailableEvent)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCachedArray)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMNotifyAudioAvailableEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNotifyAudioAvailableEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(NotifyAudioAvailableEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

nsDOMNotifyAudioAvailableEvent::~nsDOMNotifyAudioAvailableEvent()
{
  MOZ_COUNT_DTOR(nsDOMNotifyAudioAvailableEvent);
  if (mCachedArray) {
    NS_DROP_JS_OBJECTS(this, nsDOMNotifyAudioAvailableEvent);
    mCachedArray = nsnull;
  }
}

NS_IMETHODIMP
nsDOMNotifyAudioAvailableEvent::GetFrameBuffer(JSContext* aCx, jsval* aResult)
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

  mCachedArray = js_CreateTypedArray(aCx, js::TypedArray::TYPE_FLOAT32, mFrameBufferLength);
  if (!mCachedArray) {
    NS_DROP_JS_OBJECTS(this, nsDOMNotifyAudioAvailableEvent);
    NS_ERROR("Failed to get audio signal!");
    return NS_ERROR_FAILURE;
  }

  JSObject *tdest = js::TypedArray::getTypedArray(mCachedArray);
  memcpy(JS_GetTypedArrayData(tdest), mFrameBuffer.get(), mFrameBufferLength * sizeof(float));

  *aResult = OBJECT_TO_JSVAL(mCachedArray);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMNotifyAudioAvailableEvent::GetTime(float *aRetVal)
{
  *aRetVal = mTime;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMNotifyAudioAvailableEvent::InitAudioAvailableEvent(const nsAString& aType,
                                                        PRBool aCanBubble,
                                                        PRBool aCancelable,
                                                        float* aFrameBuffer,
                                                        PRUint32 aFrameBufferLength,
                                                        float aTime,
                                                        PRBool aAllowAudioData)
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
  return NS_OK;
}

nsresult NS_NewDOMAudioAvailableEvent(nsIDOMEvent** aInstancePtrResult,
                                      nsPresContext* aPresContext,
                                      nsEvent *aEvent,
                                      PRUint32 aEventType,
                                      float* aFrameBuffer,
                                      PRUint32 aFrameBufferLength,
                                      float aTime)
{
  nsDOMNotifyAudioAvailableEvent* it =
    new nsDOMNotifyAudioAvailableEvent(aPresContext, aEvent, aEventType,
                                       aFrameBuffer, aFrameBufferLength, aTime);
  return CallQueryInterface(it, aInstancePtrResult);
}
