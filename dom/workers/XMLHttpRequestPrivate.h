/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#ifndef mozilla_dom_workers_xmlhttprequestprivate_h__
#define mozilla_dom_workers_xmlhttprequestprivate_h__

#include "Workers.h"

#include "jspubtd.h"
#include "nsAutoPtr.h"

#include "EventTarget.h"
#include "WorkerFeature.h"
#include "WorkerPrivate.h"

BEGIN_WORKERS_NAMESPACE

namespace xhr {

class Proxy;

class XMLHttpRequestPrivate : public events::EventTarget,
                              public WorkerFeature
{
  JSObject* mJSObject;
  JSObject* mUploadJSObject;
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Proxy> mProxy;
  PRUint32 mJSObjectRootCount;

  bool mMultipart;
  bool mBackgroundRequest;
  bool mWithCredentials;
  bool mCanceled;

public:
  XMLHttpRequestPrivate(JSObject* aObj, WorkerPrivate* aWorkerPrivate);
  ~XMLHttpRequestPrivate();

  void
  FinalizeInstance(JSContext* aCx)
  {
    ReleaseProxy();
    events::EventTarget::FinalizeInstance(aCx);
  }

  void
  UnrootJSObject(JSContext* aCx);

  JSObject*
  GetJSObject()
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    return mJSObject;
  }

  JSObject*
  GetUploadJSObject()
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    return mUploadJSObject;
  }

  void
  SetUploadObject(JSObject* aUploadObj)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    mUploadJSObject = aUploadObj;
  }

  using events::EventTarget::TraceInstance;
  using events::EventTarget::GetEventListenerOnEventTarget;
  using events::EventTarget::SetEventListenerOnEventTarget;

  bool
  Notify(JSContext* aCx, Status aStatus);

  bool
  SetMultipart(JSContext* aCx, jsval *aVp);

  bool
  SetMozBackgroundRequest(JSContext* aCx, jsval *aVp);

  bool
  SetWithCredentials(JSContext* aCx, jsval *aVp);

  bool
  Abort(JSContext* aCx);

  JSString*
  GetAllResponseHeaders(JSContext* aCx);

  JSString*
  GetResponseHeader(JSContext* aCx, JSString* aHeader);

  bool
  Open(JSContext* aCx, JSString* aMethod, JSString* aURL, bool aAsync,
       JSString* aUser, JSString* aPassword);

  bool
  Send(JSContext* aCx, bool aHasBody, jsval aBody);

  bool
  SendAsBinary(JSContext* aCx, JSString* aBody);

  bool
  SetRequestHeader(JSContext* aCx, JSString* aHeader, JSString* aValue);

  bool
  OverrideMimeType(JSContext* aCx, JSString* aMimeType);

private:
  void
  ReleaseProxy();

  bool
  RootJSObject(JSContext* aCx);

  bool
  MaybeDispatchPrematureAbortEvents(JSContext* aCx, bool aFromOpen);

  bool
  DispatchPrematureAbortEvent(JSContext* aCx, JSObject* aTarget,
                              PRUint64 aEventType, bool aUploadTarget);

  bool
  SendInProgress() const
  {
    return mJSObjectRootCount != 0;
  }
};

}  // namespace xhr

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_xmlhttprequestprivate_h__
