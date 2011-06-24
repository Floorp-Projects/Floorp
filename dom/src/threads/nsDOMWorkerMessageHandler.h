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
 * The Original Code is Web Workers.
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

#ifndef __NSDOMWORKERMESSAGEHANDLER_H__
#define __NSDOMWORKERMESSAGEHANDLER_H__

#include "nsIClassInfo.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMWorkers.h"

#include "nsIProgrammingLanguage.h"

#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIClassInfoImpl.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "nsIWeakReference.h"

class nsDOMWorkerEventListenerBase
{
public:
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual already_AddRefed<nsIDOMEventListener> GetListener() = 0;
  virtual JSObject* GetJSObject() = 0;

protected:
  virtual ~nsDOMWorkerEventListenerBase() { }

  nsAutoRefCnt mRefCnt;
};

class nsDOMWorkerWeakEventListener : public nsDOMWorkerEventListenerBase
{
public:
  nsDOMWorkerWeakEventListener()
  : mObj(NULL) { }

  nsresult Init(nsIDOMEventListener* aListener);

  already_AddRefed<nsIDOMEventListener> GetListener();

  virtual JSObject* GetJSObject() {
    return mObj;
  }

private:
  JSObject* mObj;
};

class nsDOMWorkerWrappedWeakEventListener : public nsDOMWorkerEventListenerBase
{
public:
  nsDOMWorkerWrappedWeakEventListener(nsDOMWorkerWeakEventListener* aInner);

  already_AddRefed<nsIDOMEventListener> GetListener() {
    return mInner->GetListener();
  }

  virtual JSObject* GetJSObject() {
    return mInner->GetJSObject();
  }

private:
  nsRefPtr<nsDOMWorkerWeakEventListener> mInner;
};

class nsDOMWorkerMessageHandler : public nsIDOMEventTarget,
                                  public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTTARGET
  NS_DECL_NSICLASSINFO

  virtual nsresult SetOnXListener(const nsAString& aType,
                                  nsIDOMEventListener* aListener);

  already_AddRefed<nsIDOMEventListener>
    GetOnXListener(const nsAString& aType) const;

  void ClearListeners(const nsAString& aType);

  PRBool HasListeners(const nsAString& aType);

  void ClearAllListeners();

  void Trace(JSTracer* aTracer);

protected:
  virtual ~nsDOMWorkerMessageHandler() { }

private:

  typedef nsCOMPtr<nsIDOMEventListener> Listener;
  typedef nsTArray<Listener> ListenerArray;

  typedef nsRefPtr<nsDOMWorkerEventListenerBase> WeakListener;
  typedef nsTArray<WeakListener> WeakListenerArray;

  struct ListenerCollection {
    PRBool operator==(const ListenerCollection& aOther) const {
      return this == &aOther;
    }

    ListenerCollection(const nsAString& aType)
    : type(aType) { }

    nsString type;
    WeakListenerArray listeners;
    nsRefPtr<nsDOMWorkerWrappedWeakEventListener> onXListener;
  };

  const ListenerCollection* GetListenerCollection(const nsAString& aType) const;

  void GetListenersForType(const nsAString& aType,
                           ListenerArray& _retval) const;

  nsTArray<ListenerCollection> mCollections;
};

#define NS_FORWARD_INTERNAL_NSIDOMEVENTTARGET(_to) \
  virtual nsIDOMEventTarget * GetTargetForDOMEvent(void) { return _to GetTargetForDOMEvent(); } \
  virtual nsIDOMEventTarget * GetTargetForEventTargetChain(void) { return _to GetTargetForEventTargetChain(); } \
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor & aVisitor) { return _to PreHandleEvent(aVisitor); } \
  virtual nsresult WillHandleEvent(nsEventChainPostVisitor & aVisitor) { return _to WillHandleEvent(aVisitor); } \
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor & aVisitor) { return _to PostHandleEvent(aVisitor); } \
  virtual nsresult DispatchDOMEvent(nsEvent *aEvent, nsIDOMEvent *aDOMEvent, nsPresContext *aPresContext, nsEventStatus *aEventStatus) { return _to DispatchDOMEvent(aEvent, aDOMEvent, aPresContext, aEventStatus); } \
  virtual nsEventListenerManager * GetListenerManager(PRBool aMayCreate) { return _to GetListenerManager(aMayCreate); } \
  virtual nsresult AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID & aIID) { return _to AddEventListenerByIID(aListener, aIID); } \
  virtual nsresult RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID & aIID) { return _to RemoveEventListenerByIID(aListener, aIID); } \
  virtual nsIScriptContext * GetContextForEventHandlers(nsresult *aRv NS_OUTPARAM) { return _to GetContextForEventHandlers(aRv); } \
  virtual JSContext * GetJSContextForEventHandlers(void) { return _to GetJSContextForEventHandlers(); } 


#endif /* __NSDOMWORKERMESSAGEHANDLER_H__ */
