/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (Original Author)
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "SmsMessage.h"
#include "nsIDOMClassInfo.h"
#include "jsapi.h" // For OBJECT_TO_JSVAL and JS_NewDateObjectMsec
#include "jsdate.h" // For js_DateGetMsecSinceEpoch
#include "Constants.h"

DOMCI_DATA(MozSmsMessage, mozilla::dom::sms::SmsMessage)

namespace mozilla {
namespace dom {
namespace sms {

NS_INTERFACE_MAP_BEGIN(SmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSmsMessage)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(SmsMessage)
NS_IMPL_RELEASE(SmsMessage)

SmsMessage::SmsMessage(const SmsMessageData& aData)
  : mData(aData)
{
}

/* static */ nsresult
SmsMessage::Create(PRInt32 aId,
                   const nsAString& aDelivery,
                   const nsAString& aSender,
                   const nsAString& aReceiver,
                   const nsAString& aBody,
                   const jsval& aTimestamp,
                   JSContext* aCx,
                   nsIDOMMozSmsMessage** aMessage)
{
  *aMessage = nsnull;

  // SmsMessageData exposes these as references, so we can simply assign
  // to them.
  SmsMessageData data;
  data.id() = aId;
  data.sender() = nsString(aSender);
  data.receiver() = nsString(aReceiver);
  data.body() = nsString(aBody);

  if (aDelivery.Equals(DELIVERY_RECEIVED)) {
    data.delivery() = eDeliveryState_Received;
  } else if (aDelivery.Equals(DELIVERY_SENT)) {
    data.delivery() = eDeliveryState_Sent;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  // We support both a Date object and a millisecond timestamp as a number.
  if (aTimestamp.isObject()) {
    JSObject& obj = aTimestamp.toObject();
    if (!JS_ObjectIsDate(aCx, &obj)) {
      return NS_ERROR_INVALID_ARG;
    }
    data.timestamp() = js_DateGetMsecSinceEpoch(aCx, &obj);
  } else {
    if (!aTimestamp.isNumber()) {
      return NS_ERROR_INVALID_ARG;
    }
    jsdouble number = aTimestamp.toNumber();
    if (static_cast<PRUint64>(number) != number) {
      return NS_ERROR_INVALID_ARG;
    }
    data.timestamp() = static_cast<PRUint64>(number);
  }

  nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(data);
  message.swap(*aMessage);
  return NS_OK;
}

const SmsMessageData&
SmsMessage::GetData() const
{
  return mData;
}

NS_IMETHODIMP
SmsMessage::GetId(PRInt32* aId)
{
  *aId = mData.id();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetDelivery(nsAString& aDelivery)
{
  switch (mData.delivery()) {
    case eDeliveryState_Received:
      aDelivery.AssignLiteral("received");
      break;
    case eDeliveryState_Sent:
      aDelivery.AssignLiteral("sent");
      break;
    case eDeliveryState_Unknown:
    default:
      NS_ASSERTION(true, "We shouldn't get an unknown delivery state!");
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetSender(nsAString& aSender)
{
  aSender = mData.sender();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetReceiver(nsAString& aReceiver)
{
  aReceiver = mData.receiver();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetBody(nsAString& aBody)
{
  aBody = mData.body();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetTimestamp(JSContext* cx, jsval* aDate)
{
  *aDate = OBJECT_TO_JSVAL(JS_NewDateObjectMsec(cx, mData.timestamp()));
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
