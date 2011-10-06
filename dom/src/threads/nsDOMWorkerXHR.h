/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#ifndef __NSDOMWORKERXHR_H__
#define __NSDOMWORKERXHR_H__

// Bases
#include "nsIClassInfo.h"
#include "nsIXMLHttpRequest.h"
#include "nsIXPCScriptable.h"

// Other includes
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

// DOMWorker includes
#include "nsDOMWorker.h"
#include "nsDOMWorkerMacros.h"
#include "nsDOMWorkerXHRProxy.h"

// Convenience defines for event *indexes* in the sListenerTypes array.
#define LISTENER_TYPE_ABORT 0
#define LISTENER_TYPE_ERROR 1
#define LISTENER_TYPE_LOAD 2
#define LISTENER_TYPE_LOADSTART 3
#define LISTENER_TYPE_PROGRESS 4
#define LISTENER_TYPE_READYSTATECHANGE 5
#define LISTENER_TYPE_LOADEND 6

class nsIXPConnectWrappedNative;

class nsDOMWorkerXHREventTarget : public nsDOMWorkerMessageHandler,
                                  public nsIXMLHttpRequestEventTarget
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMWorkerMessageHandler::)
  NS_DECL_NSIXMLHTTPREQUESTEVENTTARGET

  static const char* const sListenerTypes[];
  static const PRUint32 sMaxXHREventTypes;
  static const PRUint32 sMaxUploadEventTypes;

  static PRUint32 GetListenerTypeFromString(const nsAString& aString);

protected:
  virtual ~nsDOMWorkerXHREventTarget() { }
};

class nsDOMWorkerXHRUpload;

class nsDOMWorkerXHR : public nsDOMWorkerFeature,
                       public nsDOMWorkerXHREventTarget,
                       public nsIXMLHttpRequest,
                       public nsIXPCScriptable
{
  typedef mozilla::Mutex Mutex;

  friend class nsDOMWorkerXHREvent;
  friend class nsDOMWorkerXHRLastProgressOrLoadEvent;
  friend class nsDOMWorkerXHRProxy;
  friend class nsDOMWorkerXHRUpload;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXMLHTTPREQUEST
  NS_FORWARD_NSICLASSINFO_NOGETINTERFACES(nsDOMWorkerXHREventTarget::)
  NS_DECL_NSICLASSINFO_GETINTERFACES
  NS_DECL_NSIXPCSCRIPTABLE

  nsDOMWorkerXHR(nsDOMWorker* aWorker);

  nsresult Init();

  virtual void Cancel();

  virtual nsresult SetOnXListener(const nsAString& aType,
                                  nsIDOMEventListener* aListener);

private:
  virtual ~nsDOMWorkerXHR();

  Mutex& GetLock() {
    return mWorker->GetLock();
  }

  already_AddRefed<nsIXPConnectWrappedNative> GetWrappedNative() {
    nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative(mWrappedNative);
    return wrappedNative.forget();
  }

  nsRefPtr<nsDOMWorkerXHRProxy> mXHRProxy;
  nsRefPtr<nsDOMWorkerXHRUpload> mUpload;

  nsIXPConnectWrappedNative* mWrappedNative;

  volatile bool mCanceled;
};

class nsDOMWorkerXHRUpload : public nsDOMWorkerXHREventTarget,
                             public nsIXMLHttpRequestUpload
{
  friend class nsDOMWorkerXHR;

public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMEventHandler
  NS_FORWARD_INTERNAL_NSIDOMEVENTTARGET(nsDOMWorkerMessageHandler::)
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture,
                              bool aWantsUntrusted,
                              PRUint8 optional_argc);
  NS_IMETHOD RemoveEventListener(const nsAString& aType,
                                 nsIDOMEventListener* aListener,
                                 bool aUseCapture);
  NS_IMETHOD DispatchEvent(nsIDOMEvent* aEvent,
                           bool* _retval);
  NS_FORWARD_NSIXMLHTTPREQUESTEVENTTARGET(nsDOMWorkerXHREventTarget::)
  NS_DECL_NSIXMLHTTPREQUESTUPLOAD
  NS_FORWARD_NSICLASSINFO_NOGETINTERFACES(nsDOMWorkerXHREventTarget::)
  NS_DECL_NSICLASSINFO_GETINTERFACES

  nsDOMWorkerXHRUpload(nsDOMWorkerXHR* aWorkerXHR);

  virtual nsresult SetOnXListener(const nsAString& aType,
                                  nsIDOMEventListener* aListener);

protected:
  virtual ~nsDOMWorkerXHRUpload() { }

  nsRefPtr<nsDOMWorkerXHR> mWorkerXHR;
};

#endif /* __NSDOMWORKERXHR_H__ */
