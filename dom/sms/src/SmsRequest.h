/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsRequest_h
#define mozilla_dom_sms_SmsRequest_h

#include "nsIDOMSmsRequest.h"
#include "nsDOMEventTargetHelper.h"

class nsIDOMMozSmsMessage;
class nsIDOMMozSmsCursor;

namespace mozilla {
namespace dom {
namespace sms {
class SmsManager;

class SmsRequest : public nsDOMEventTargetHelper
                 , public nsIDOMMozSmsRequest
{
public:
  friend class SmsRequestManager;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMREQUEST
  NS_DECL_NSIDOMMOZSMSREQUEST

  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(SmsRequest,
                                                         nsDOMEventTargetHelper)

  void Reset();

private:
  SmsRequest() MOZ_DELETE;

  SmsRequest(SmsManager* aManager);
  ~SmsRequest();

  /**
   * Root mResult (jsval) to prevent garbage collection.
   */
  void RootResult();

  /**
   * Unroot mResult (jsval) to allow garbage collection.
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
   * Set the object in an error state with the error type being aError.
   */
  void SetError(PRInt32 aError);

  /**
   * Set the object in a success state with the result being the nsISupports
   * object in parameter.
   * @return whether setting the object was a success
   */
  bool SetSuccessInternal(nsISupports* aObject);

  /**
   * Return the internal cursor that is saved when
   * SetSuccess(nsIDOMMozSmsCursor*) is used.
   * Returns null if this request isn't associated to an cursor.
   */
  nsIDOMMozSmsCursor* GetCursor();

  jsval     mResult;
  bool      mResultRooted;
  bool      mDone;
  nsCOMPtr<nsIDOMDOMError> mError;
  nsCOMPtr<nsIDOMMozSmsCursor> mCursor;

  NS_DECL_EVENT_HANDLER(success)
  NS_DECL_EVENT_HANDLER(error)
};

inline nsIDOMMozSmsCursor*
SmsRequest::GetCursor()
{
  return mCursor;
}

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsRequest_h
