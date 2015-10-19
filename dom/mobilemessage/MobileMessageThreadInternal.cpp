/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageThreadInternal.h"
#include "nsIDOMClassInfo.h"
#include "jsapi.h"           // For OBJECT_TO_JSVAL and JS_NewDateObjectMsec
#include "nsJSUtils.h"       // For nsAutoJSString
#include "nsTArrayHelpers.h" // For nsTArrayToJSArray
#include "mozilla/dom/mobilemessage/Constants.h" // For MessageType

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS(MobileMessageThreadInternal, nsIMobileMessageThread)

/* static */ nsresult
MobileMessageThreadInternal::Create(uint64_t aId,
                                    const JS::Value& aParticipants,
                                    uint64_t aTimestamp,
                                    const nsAString& aLastMessageSubject,
                                    const nsAString& aBody,
                                    uint64_t aUnreadCount,
                                    const nsAString& aLastMessageType,
                                    JSContext* aCx,
                                    nsIMobileMessageThread** aThread)
{
  *aThread = nullptr;

  // ThreadData exposes these as references, so we can simply assign
  // to them.
  ThreadData data;
  data.id() = aId;
  data.lastMessageSubject().Assign(aLastMessageSubject);
  data.body().Assign(aBody);
  data.unreadCount() = aUnreadCount;

  // Participants.
  {
    if (!aParticipants.isObject()) {
      return NS_ERROR_INVALID_ARG;
    }

    JS::Rooted<JSObject*> obj(aCx, &aParticipants.toObject());
    bool isArray;
    if (!JS_IsArrayObject(aCx, obj, &isArray)) {
      return NS_ERROR_FAILURE;
    }
    if (!isArray) {
      return NS_ERROR_INVALID_ARG;
    }

    uint32_t length;
    MOZ_ALWAYS_TRUE(JS_GetArrayLength(aCx, obj, &length));
    NS_ENSURE_TRUE(length, NS_ERROR_INVALID_ARG);

    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> val(aCx);

      if (!JS_GetElement(aCx, obj, i, &val) || !val.isString()) {
        return NS_ERROR_INVALID_ARG;
      }

      nsAutoJSString str;
      if (!str.init(aCx, val.toString())) {
        return NS_ERROR_FAILURE;
      }

      data.participants().AppendElement(str);
    }
  }

  // Set |timestamp|;
  data.timestamp() = aTimestamp;

  // Set |lastMessageType|.
  {
    MessageType lastMessageType;
    if (aLastMessageType.Equals(MESSAGE_TYPE_SMS)) {
      lastMessageType = eMessageType_SMS;
    } else if (aLastMessageType.Equals(MESSAGE_TYPE_MMS)) {
      lastMessageType = eMessageType_MMS;
    } else {
      return NS_ERROR_INVALID_ARG;
    }
    data.lastMessageType() = lastMessageType;
  }

  nsCOMPtr<nsIMobileMessageThread> thread =
    new MobileMessageThreadInternal(data);
  thread.forget(aThread);
  return NS_OK;
}

MobileMessageThreadInternal::MobileMessageThreadInternal(uint64_t aId,
                                                         const nsTArray<nsString>& aParticipants,
                                                         uint64_t aTimestamp,
                                                         const nsString& aLastMessageSubject,
                                                         const nsString& aBody,
                                                         uint64_t aUnreadCount,
                                                         MessageType aLastMessageType)
  : mData(aId, aParticipants, aTimestamp, aLastMessageSubject, aBody,
          aUnreadCount, aLastMessageType)
{
  MOZ_ASSERT(aParticipants.Length());
}

MobileMessageThreadInternal::MobileMessageThreadInternal(const ThreadData& aData)
  : mData(aData)
{
  MOZ_ASSERT(aData.participants().Length());
}

NS_IMETHODIMP
MobileMessageThreadInternal::GetId(uint64_t* aId)
{
  *aId = mData.id();
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThreadInternal::GetLastMessageSubject(nsAString& aLastMessageSubject)
{
  aLastMessageSubject = mData.lastMessageSubject();
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThreadInternal::GetBody(nsAString& aBody)
{
  aBody = mData.body();
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThreadInternal::GetUnreadCount(uint64_t* aUnreadCount)
{
  *aUnreadCount = mData.unreadCount();
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThreadInternal::GetParticipants(JSContext* aCx,
                                             JS::MutableHandle<JS::Value> aParticipants)
{
  JS::Rooted<JSObject*> obj(aCx);

  nsresult rv = nsTArrayToJSArray(aCx, mData.participants(), &obj);
  NS_ENSURE_SUCCESS(rv, rv);

  aParticipants.setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThreadInternal::GetTimestamp(DOMTimeStamp* aDate)
{
  *aDate = mData.timestamp();
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageThreadInternal::GetLastMessageType(nsAString& aLastMessageType)
{
  switch (mData.lastMessageType()) {
    case eMessageType_SMS:
      aLastMessageType = MESSAGE_TYPE_SMS;
      break;
    case eMessageType_MMS:
      aLastMessageType = MESSAGE_TYPE_MMS;
      break;
    case eMessageType_EndGuard:
    default:
      MOZ_CRASH("We shouldn't get any other message type!");
  }

  return NS_OK;
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
