/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsRequest_h
#define mozilla_dom_mobilemessage_SmsRequest_h

#include "nsIDOMSmsRequest.h"
#include "nsIMobileMessageCallback.h"
#include "nsDOMEventTargetHelper.h"

class nsIDOMMozSmsMessage;
class nsIDOMMozSmsCursor;

namespace mozilla {
namespace dom {

namespace mobilemessage {
  class SmsRequestChild;
  class SmsRequestParent;
  class MessageReply;
  class ThreadListItem;
}

// We need this forwarder to avoid a QI to nsIClassInfo.
// See: https://bugzilla.mozilla.org/show_bug.cgi?id=775997#c51 
class SmsRequestForwarder : public nsIMobileMessageCallback
{
  friend class mobilemessage::SmsRequestChild;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIMOBILEMESSAGECALLBACK(mRealRequest->)

  SmsRequestForwarder(nsIMobileMessageCallback* aRealRequest) {
    mRealRequest = aRealRequest;
  }

private:
  virtual
  ~SmsRequestForwarder() {}

  nsIMobileMessageCallback* GetRealRequest() {
    return mRealRequest;
  }

  nsCOMPtr<nsIMobileMessageCallback> mRealRequest;
};

class SmsManager;
class MobileMessageManager;

class SmsRequest : public nsDOMEventTargetHelper
                 , public nsIDOMMozSmsRequest
                 , public nsIMobileMessageCallback
{
public:
  friend class SmsCursor;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMREQUEST
  NS_DECL_NSIMOBILEMESSAGECALLBACK
  NS_DECL_NSIDOMMOZSMSREQUEST

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(SmsRequest,
                                                         nsDOMEventTargetHelper)

  static already_AddRefed<nsIDOMMozSmsRequest> Create(SmsManager* aManager);
  static already_AddRefed<nsIDOMMozSmsRequest> Create(MobileMessageManager* aManager);

  static already_AddRefed<SmsRequest> Create(mobilemessage::SmsRequestParent* requestParent);
  void Reset();

  void SetActorDied() {
    mParentAlive = false;
  }

  void
  NotifyThreadList(const InfallibleTArray<mobilemessage::ThreadListItem>& aItems);

private:
  SmsRequest() MOZ_DELETE;

  SmsRequest(SmsManager* aManager);
  SmsRequest(MobileMessageManager* aManager);
  SmsRequest(mobilemessage::SmsRequestParent* aParent);
  ~SmsRequest();

  nsresult SendMessageReply(const mobilemessage::MessageReply& aReply);

  /**
   * Root mResult (JS::Value) to prevent garbage collection.
   */
  void RootResult();

  /**
   * Unroot mResult (JS::Value) to allow garbage collection.
   */
  void UnrootResult();

  /**
   * Set the object in a success state with the result being aMessage.
   */
  void SetSuccess(nsIDOMMozSmsMessage* aMessage);

  /**
   * Set the object in a success state with the result being a boolean.
   */
  void SetSuccess(bool aResult);

  /**
   * Set the object in a success state with the result being a SmsCursor.
   */
  void SetSuccess(nsIDOMMozSmsCursor* aCursor);

  /**
   * Set the object in a success state with the result being the given JS::Value.
   */
  void SetSuccess(const JS::Value& aVal);

  /**
   * Set the object in an error state with the error type being aError.
   */
  void SetError(int32_t aError);

  /**
   * Set the object in a success state with the result being the nsISupports
   * object in parameter.
   * @return whether setting the object was a success
   */
  bool SetSuccessInternal(nsISupports* aObject);

  nsresult DispatchTrustedEvent(const nsAString& aEventName);

  template <class T>
  nsresult NotifySuccess(T aParam);
  nsresult NotifyError(int32_t aError);

  JS::Value mResult;
  bool      mResultRooted;
  bool      mDone;
  bool      mParentAlive;
  mobilemessage::SmsRequestParent* mParent;
  nsCOMPtr<nsIDOMDOMError> mError;
  nsCOMPtr<nsIDOMMozSmsCursor> mCursor;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsRequest_h
