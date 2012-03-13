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

class SmsRequest : public nsIDOMMozSmsRequest
                 , public nsDOMEventTargetHelper
{
public:
  friend class SmsRequestManager;

  NS_DECL_ISUPPORTS
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
  PRInt32   mError;
  bool      mDone;
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
