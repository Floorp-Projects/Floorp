/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsRequest.h"
#include "nsIDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsDOMString.h"
#include "nsContentUtils.h"
#include "nsIDOMSmsMessage.h"
#include "SmsCursor.h"
#include "SmsMessage.h"
#include "SmsManager.h"
#include "mozilla/dom/DOMError.h"
#include "SmsParent.h"

#define SUCCESS_EVENT_NAME NS_LITERAL_STRING("success")
#define ERROR_EVENT_NAME   NS_LITERAL_STRING("error")

DOMCI_DATA(MozSmsRequest, mozilla::dom::sms::SmsRequest)

namespace mozilla {
namespace dom {
namespace sms {

NS_IMPL_ISUPPORTS1(SmsRequestForwarder, nsISmsRequest)

NS_IMPL_CYCLE_COLLECTION_CLASS(SmsRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SmsRequest,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCursor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SmsRequest,
                                                nsDOMEventTargetHelper)
  if (tmp->mResultRooted) {
    tmp->mResult = JSVAL_VOID;
    tmp->UnrootResult();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCursor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(SmsRequest,
                                               nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mResult)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SmsRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSmsRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMRequest)
  NS_INTERFACE_MAP_ENTRY(nsISmsRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozSmsRequest)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSmsRequest)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(SmsRequest, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SmsRequest, nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(SmsRequest, success)
NS_IMPL_EVENT_HANDLER(SmsRequest, error)

already_AddRefed<nsIDOMMozSmsRequest>
SmsRequest::Create(SmsManager* aManager)
{
  nsCOMPtr<nsIDOMMozSmsRequest> request = new SmsRequest(aManager);
  return request.forget();
}

already_AddRefed<SmsRequest>
SmsRequest::Create(SmsRequestParent* aRequestParent)
{
  nsRefPtr<SmsRequest> request = new SmsRequest(aRequestParent);
  return request.forget();
}

SmsRequest::SmsRequest(SmsManager* aManager)
  : mResult(JSVAL_VOID)
  , mResultRooted(false)
  , mDone(false)
  , mParentAlive(false)
  , mParent(nullptr)
{
  BindToOwner(aManager);
}

SmsRequest::SmsRequest(SmsRequestParent* aRequestParent)
  : mResult(JSVAL_VOID)
  , mResultRooted(false)
  , mDone(false)
  , mParentAlive(true)
  , mParent(aRequestParent)
{
  MOZ_ASSERT(aRequestParent);
}

SmsRequest::~SmsRequest()
{
  if (mResultRooted) {
    UnrootResult();
  }
}

void
SmsRequest::Reset()
{
  NS_ASSERTION(mDone, "mDone should be true if we try to reset!");
  NS_ASSERTION(mResult != JSVAL_VOID, "mResult should be set if we try to reset!");
  NS_ASSERTION(!mError, "There should be no error if we try to reset!");

  if (mResultRooted) {
    UnrootResult();
  }

  mResult = JSVAL_VOID;
  mDone = false;
}

void
SmsRequest::RootResult()
{
  NS_ASSERTION(!mResultRooted, "Don't call RootResult() if already rooted!");
  NS_HOLD_JS_OBJECTS(this, SmsRequest);
  mResultRooted = true;
}

void
SmsRequest::UnrootResult()
{
  NS_ASSERTION(mResultRooted, "Don't call UnrotResult() if not rooted!");
  NS_DROP_JS_OBJECTS(this, SmsRequest);
  mResultRooted = false;
}

void
SmsRequest::SetSuccess(nsIDOMMozSmsMessage* aMessage)
{
  SetSuccessInternal(aMessage);
}

void
SmsRequest::SetSuccess(bool aResult)
{
  NS_PRECONDITION(!mDone, "mDone shouldn't have been set to true already!");
  NS_PRECONDITION(!mError, "mError shouldn't have been set!");
  NS_PRECONDITION(mResult == JSVAL_NULL, "mResult shouldn't have been set!");

  mResult.setBoolean(aResult);
  mDone = true;
}

void
SmsRequest::SetSuccess(nsIDOMMozSmsCursor* aCursor)
{
  if (!SetSuccessInternal(aCursor)) {
    return;
  }

  NS_ASSERTION(!mCursor || mCursor == aCursor,
               "SmsRequest can't change it's cursor!");

  if (!mCursor) {
    mCursor = aCursor;
  }
}

bool
SmsRequest::SetSuccessInternal(nsISupports* aObject)
{
  NS_PRECONDITION(!mDone, "mDone shouldn't have been set to true already!");
  NS_PRECONDITION(!mError, "mError shouldn't have been set!");
  NS_PRECONDITION(mResult == JSVAL_VOID, "mResult shouldn't have been set!");

  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  if (!sc) {
    SetError(nsISmsRequest::INTERNAL_ERROR);
    return false;
  }

  JSContext* cx = sc->GetNativeContext();    
  NS_ASSERTION(cx, "Failed to get a context!");

  JSObject* global = sc->GetNativeGlobal();
  NS_ASSERTION(global, "Failed to get global object!");

  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, global);

  RootResult();

  if (NS_FAILED(nsContentUtils::WrapNative(cx, global, aObject, &mResult))) {
    UnrootResult();
    mResult = JSVAL_VOID;
    SetError(nsISmsRequest::INTERNAL_ERROR);
    return false;
  }

  mDone = true;
  return true;
}

void
SmsRequest::SetError(int32_t aError)
{
  NS_PRECONDITION(!mDone, "mDone shouldn't have been set to true already!");
  NS_PRECONDITION(!mError, "mError shouldn't have been set!");
  NS_PRECONDITION(mResult == JSVAL_VOID, "mResult shouldn't have been set!");
  NS_PRECONDITION(aError != nsISmsRequest::SUCCESS_NO_ERROR,
                  "Can't call SetError() with SUCCESS_NO_ERROR!");

  mDone = true;
  mCursor = nullptr;

  switch (aError) {
    case nsISmsRequest::NO_SIGNAL_ERROR:
      mError = DOMError::CreateWithName(NS_LITERAL_STRING("NoSignalError"));
      break;
    case nsISmsRequest::NOT_FOUND_ERROR:
      mError = DOMError::CreateWithName(NS_LITERAL_STRING("NotFoundError"));
      break;
    case nsISmsRequest::UNKNOWN_ERROR:
      mError = DOMError::CreateWithName(NS_LITERAL_STRING("UnknownError"));
      break;
    case nsISmsRequest::INTERNAL_ERROR:
      mError = DOMError::CreateWithName(NS_LITERAL_STRING("InternalError"));
      break;
    default: // SUCCESS_NO_ERROR is handled above.
      MOZ_ASSERT(false, "Unknown error value.");
  }
}

NS_IMETHODIMP
SmsRequest::GetReadyState(nsAString& aReadyState)
{
  if (mDone) {
    aReadyState.AssignLiteral("done");
  } else {
    aReadyState.AssignLiteral("processing");
  }

  return NS_OK;
}

NS_IMETHODIMP
SmsRequest::GetError(nsIDOMDOMError** aError)
{
  NS_ASSERTION(mDone || !mError, "mError should be null when pending");
  NS_ASSERTION(!mError || mResult == JSVAL_VOID,
               "mResult should be void when there is an error!");

  NS_IF_ADDREF(*aError = mError);

  return NS_OK;
}

NS_IMETHODIMP
SmsRequest::GetResult(jsval* aResult)
{
  if (!mDone) {
    NS_ASSERTION(mResult == JSVAL_VOID,
                 "When not done, result should be null!");

    *aResult = JSVAL_VOID;
    return NS_OK;
  }

  *aResult = mResult;
  return NS_OK;
}

nsresult
SmsRequest::DispatchTrustedEvent(const nsAString& aEventName)
{
  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nullptr, nullptr);
  nsresult rv = event->InitEvent(aEventName, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  return DispatchEvent(event, &dummy);
}

template <class T>
nsresult
SmsRequest::NotifySuccess(T aParam)
{
  SetSuccess(aParam);
  nsresult rv = DispatchTrustedEvent(SUCCESS_EVENT_NAME);
  return rv;
}

nsresult
SmsRequest::NotifyError(int32_t aError)
{
  SetError(aError);

  nsresult rv = DispatchTrustedEvent(ERROR_EVENT_NAME);
  return rv;
}

nsresult
SmsRequest::SendMessageReply(const MessageReply& aReply)
{
  if (mParentAlive) {
    mParent->SendReply(aReply);
    mParent = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
SmsRequest::NotifyMessageSent(nsIDOMMozSmsMessage *aMessage)
{
  if (mParent) {
    SmsMessageData data = SmsMessageData(static_cast<SmsMessage*>(aMessage)->GetData());
    return SendMessageReply(MessageReply(ReplyMessageSend(data)));
  }
  return NotifySuccess<nsIDOMMozSmsMessage*>(aMessage);
}

NS_IMETHODIMP
SmsRequest::NotifySendMessageFailed(int32_t aError)
{
  if (mParent) {
    return SendMessageReply(MessageReply(ReplyMessageSendFail(aError)));
  }
  return NotifyError(aError);

}

NS_IMETHODIMP
SmsRequest::NotifyMessageGot(nsIDOMMozSmsMessage *aMessage)
{
  if (mParent) {
    SmsMessageData data = SmsMessageData(static_cast<SmsMessage*>(aMessage)->GetData());
    return SendMessageReply(MessageReply(ReplyGetMessage(data)));
  }
  return NotifySuccess<nsIDOMMozSmsMessage*>(aMessage);

}

NS_IMETHODIMP
SmsRequest::NotifyGetMessageFailed(int32_t aError)
{
  if (mParent) {
    return SendMessageReply(MessageReply(ReplyGetMessageFail(aError)));
  }
  return NotifyError(aError);
}

NS_IMETHODIMP
SmsRequest::NotifyMessageDeleted(bool aDeleted)
{
  if (mParent) {
    return SendMessageReply(MessageReply(ReplyMessageDelete(aDeleted)));
  }
  return NotifySuccess<bool>(aDeleted);
}

NS_IMETHODIMP
SmsRequest::NotifyDeleteMessageFailed(int32_t aError)
{
  if (mParent) {
    return SendMessageReply(MessageReply(ReplyMessageDeleteFail(aError)));
  }
  return NotifyError(aError);
}

NS_IMETHODIMP
SmsRequest::NotifyMessageListCreated(int32_t aListId,
                                     nsIDOMMozSmsMessage *aMessage)
{
  if (mParent) {
    SmsMessageData data = SmsMessageData(static_cast<SmsMessage*>(aMessage)->GetData());
    return SendMessageReply(MessageReply(ReplyCreateMessageList(aListId, data)));
  } else {
    nsCOMPtr<SmsCursor> cursor = new SmsCursor(aListId, this);
    cursor->SetMessage(aMessage);
    return NotifySuccess<nsIDOMMozSmsCursor*>(cursor);
  }
}

NS_IMETHODIMP
SmsRequest::NotifyReadMessageListFailed(int32_t aError)
{
  if (mParent) {
    return SendMessageReply(MessageReply(ReplyCreateMessageListFail(aError)));
  }
  if (mCursor) {
    static_cast<SmsCursor*>(mCursor.get())->Disconnect();
  }
  return NotifyError(aError);
}

NS_IMETHODIMP
SmsRequest::NotifyNextMessageInListGot(nsIDOMMozSmsMessage *aMessage)
{
  if (mParent) {
    SmsMessageData data = SmsMessageData(static_cast<SmsMessage*>(aMessage)->GetData());
    return SendMessageReply(MessageReply(ReplyGetNextMessage(data)));
  }
  nsCOMPtr<SmsCursor> cursor = static_cast<SmsCursor*>(mCursor.get());
  NS_ASSERTION(cursor, "Request should have an cursor in that case!");
  cursor->SetMessage(aMessage);
  return NotifySuccess<nsIDOMMozSmsCursor*>(cursor);
}

NS_IMETHODIMP
SmsRequest::NotifyNoMessageInList()
{
  if (mParent) {
    return SendMessageReply(MessageReply(ReplyNoMessageInList()));
  }
  nsCOMPtr<nsIDOMMozSmsCursor> cursor = mCursor;
  if (!cursor) {
    cursor = new SmsCursor();
  } else {
    static_cast<SmsCursor*>(cursor.get())->Disconnect();
  }
  return NotifySuccess<nsIDOMMozSmsCursor*>(cursor);
}

NS_IMETHODIMP
SmsRequest::NotifyMessageMarkedRead(bool aRead)
{
  if (mParent) {
    return SendMessageReply(MessageReply(ReplyMarkeMessageRead(aRead)));
  }
  return NotifySuccess<bool>(aRead);
}

NS_IMETHODIMP
SmsRequest::NotifyMarkMessageReadFailed(int32_t aError)
{
  if (mParent) {
    return SendMessageReply(MessageReply(ReplyMarkeMessageReadFail(aError)));
  }
  return NotifyError(aError);
}

} // namespace sms
} // namespace dom
} // namespace mozilla
