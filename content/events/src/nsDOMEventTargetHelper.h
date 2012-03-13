/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Olli Pettay <Olli.Pettay@helsinki.fi>
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

#ifndef nsDOMEventTargetHelper_h_
#define nsDOMEventTargetHelper_h_

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsEventListenerManager.h"
#include "nsIScriptContext.h"
#include "nsWrapperCache.h"

class nsDOMEventListenerWrapper : public nsIDOMEventListener
{
public:
  nsDOMEventListenerWrapper(nsIDOMEventListener* aListener)
  : mListener(aListener) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMEventListenerWrapper)

  NS_DECL_NSIDOMEVENTLISTENER

  nsIDOMEventListener* GetInner() { return mListener; }
  void Disconnect() { mListener = nsnull; }
protected:
  nsCOMPtr<nsIDOMEventListener> mListener;
};

class nsDOMEventTargetHelper : public nsIDOMEventTarget,
                               public nsWrapperCache
{
public:
  nsDOMEventTargetHelper() : mOwner(nsnull), mHasOrHasHadOwner(false) {}
  virtual ~nsDOMEventTargetHelper();
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMEventTargetHelper)

  NS_DECL_NSIDOMEVENTTARGET

  void GetParentObject(nsIScriptGlobalObject **aParentObject)
  {
    if (mOwner) {
      CallQueryInterface(mOwner, aParentObject);
    }
    else {
      *aParentObject = nsnull;
    }
  }

  static nsDOMEventTargetHelper* FromSupports(nsISupports* aSupports)
  {
    nsIDOMEventTarget* target =
      static_cast<nsIDOMEventTarget*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMEventTarget> target_qi =
        do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMEventTarget pointer as the
      // nsISupports pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(target_qi == target, "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMEventTargetHelper*>(target);
  }

  void Init(JSContext* aCx = nsnull);

  bool HasListenersFor(const nsAString& aType)
  {
    return mListenerManager && mListenerManager->HasListenersFor(aType);
  }
  nsresult RemoveAddEventListener(const nsAString& aType,
                                  nsRefPtr<nsDOMEventListenerWrapper>& aCurrent,
                                  nsIDOMEventListener* aNew);

  nsresult GetInnerEventListener(nsRefPtr<nsDOMEventListenerWrapper>& aWrapper,
                                 nsIDOMEventListener** aListener);

  nsresult CheckInnerWindowCorrectness()
  {
    NS_ENSURE_STATE(!mHasOrHasHadOwner || mOwner);
    if (mOwner) {
      NS_ASSERTION(mOwner->IsInnerWindow(), "Should have inner window here!\n");
      nsPIDOMWindow* outer = mOwner->GetOuterWindow();
      if (!outer || outer->GetCurrentInnerWindow() != mOwner) {
        return NS_ERROR_FAILURE;
      }
    }
    return NS_OK;
  }

  void BindToOwner(nsPIDOMWindow* aOwner);
  void BindToOwner(nsDOMEventTargetHelper* aOther);
  virtual void DisconnectFromOwner();                   
  nsPIDOMWindow* GetOwner() { return mOwner; }
  bool HasOrHasHadOwner() { return mHasOrHasHadOwner; }
protected:
  nsRefPtr<nsEventListenerManager> mListenerManager;
private:
  // These may be null (native callers or xpcshell).
  nsPIDOMWindow*             mOwner; // Inner window.
  bool                       mHasOrHasHadOwner;
};

#define NS_DECL_EVENT_HANDLER(_event)                                         \
  protected:                                                                  \
    nsRefPtr<nsDOMEventListenerWrapper> mOn##_event##Listener;                \
  public:

#define NS_DECL_AND_IMPL_EVENT_HANDLER(_event)                                \
  protected:                                                                  \
    nsRefPtr<nsDOMEventListenerWrapper> mOn##_event##Listener;                \
  public:                                                                     \
    NS_IMETHOD GetOn##_event(nsIDOMEventListener** a##_event)                 \
    {                                                                         \
      return GetInnerEventListener(mOn##_event##Listener, a##_event);         \
    }                                                                         \
    NS_IMETHOD SetOn##_event(nsIDOMEventListener* a##_event)                  \
    {                                                                         \
      return RemoveAddEventListener(NS_LITERAL_STRING(#_event),               \
                                    mOn##_event##Listener, a##_event);        \
    }

#define NS_IMPL_EVENT_HANDLER(_class, _event)                                 \
  NS_IMETHODIMP                                                               \
  _class::GetOn##_event(nsIDOMEventListener** a##_event)                      \
  {                                                                           \
    return GetInnerEventListener(mOn##_event##Listener, a##_event);           \
  }                                                                           \
  NS_IMETHODIMP                                                               \
  _class::SetOn##_event(nsIDOMEventListener* a##_event)                       \
  {                                                                           \
    return RemoveAddEventListener(NS_LITERAL_STRING(#_event),                 \
                                  mOn##_event##Listener, a##_event);          \
  }

#define NS_IMPL_FORWARD_EVENT_HANDLER(_class, _event, _baseclass)             \
    NS_IMETHODIMP                                                             \
    _class::GetOn##_event(nsIDOMEventListener** a##_event)                    \
    {                                                                         \
      return _baseclass::GetOn##_event(a##_event);                           \
    }                                                                         \
    NS_IMETHODIMP                                                             \
    _class::SetOn##_event(nsIDOMEventListener* a##_event)                     \
    {                                                                         \
      return _baseclass::SetOn##_event(a##_event);                            \
    }

#define NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(_event)                    \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOn##_event##Listener)

#define NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(_event)                      \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOn##_event##Listener)

#define NS_DISCONNECT_EVENT_HANDLER(_event)                                   \
  if (mOn##_event##Listener) { mOn##_event##Listener->Disconnect(); }

#define NS_UNMARK_LISTENER_WRAPPER(_event)                       \
  if (tmp->mOn##_event##Listener) {                              \
    nsCOMPtr<nsIXPConnectWrappedJS> wjs =                        \
      do_QueryInterface(tmp->mOn##_event##Listener->GetInner()); \
    xpc_UnmarkGrayObject(wjs);                                   \
  }

#endif // nsDOMEventTargetHelper_h_
