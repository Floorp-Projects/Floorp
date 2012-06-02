/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbrequest_h__
#define mozilla_dom_indexeddb_idbrequest_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIIDBRequest.h"
#include "nsIIDBOpenDBRequest.h"
#include "nsDOMEventTargetHelper.h"
#include "mozilla/dom/indexedDB/IDBWrapperCache.h"

class nsIScriptContext;
class nsPIDOMWindow;

BEGIN_INDEXEDDB_NAMESPACE

class HelperBase;
class IDBTransaction;
class IndexedDBRequestParentBase;

class IDBRequest : public IDBWrapperCache,
                   public nsIIDBRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIDBREQUEST
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(IDBRequest,
                                                         IDBWrapperCache)

  static
  already_AddRefed<IDBRequest> Create(nsISupports* aSource,
                                      IDBWrapperCache* aOwnerCache,
                                      IDBTransaction* aTransaction);

  // nsIDOMEventTarget
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  nsISupports* Source()
  {
    return mSource;
  }

  void Reset();

  nsresult NotifyHelperCompleted(HelperBase* aHelper);
  void NotifyHelperSentResultsToChildProcess(nsresult aRv);

  void SetError(nsresult aRv);

  nsresult
  GetErrorCode() const
#ifdef DEBUG
  ;
#else
  {
    return mErrorCode;
  }
#endif

  JSContext* GetJSContext();

  void
  SetActor(IndexedDBRequestParentBase* aActorParent)
  {
    NS_ASSERTION(!aActorParent || !mActorParent,
                 "Shouldn't have more than one!");
    mActorParent = aActorParent;
  }

  IndexedDBRequestParentBase*
  GetActorParent() const
  {
    return mActorParent;
  }

protected:
  IDBRequest();
  ~IDBRequest();

  virtual void RootResultValInternal();
  virtual void UnrootResultValInternal();

  void RootResultVal()
  {
    if (!mRooted) {
      RootResultValInternal();
      mRooted = true;
    }
  }

  void UnrootResultVal()
  {
    if (mRooted) {
      UnrootResultValInternal();
      mRooted = false;
    }
  }

  nsCOMPtr<nsISupports> mSource;
  nsRefPtr<IDBTransaction> mTransaction;

  NS_DECL_EVENT_HANDLER(success)
  NS_DECL_EVENT_HANDLER(error)

  jsval mResultVal;

  nsCOMPtr<nsIDOMDOMError> mError;

  IndexedDBRequestParentBase* mActorParent;

  nsresult mErrorCode;
  bool mHaveResultOrErrorCode;
  bool mRooted;
};

class IDBOpenDBRequest : public IDBRequest,
                         public nsIIDBOpenDBRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIIDBREQUEST(IDBRequest::)
  NS_DECL_NSIIDBOPENDBREQUEST
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBOpenDBRequest, IDBRequest)

  static
  already_AddRefed<IDBOpenDBRequest>
  Create(nsPIDOMWindow* aOwner,
         JSObject* aScriptOwner);

  static
  already_AddRefed<IDBOpenDBRequest>
  Create(IDBWrapperCache* aOwnerCache)
  {
    return Create(aOwnerCache->GetOwner(),
                  aOwnerCache->GetScriptOwner());
  }

  void SetTransaction(IDBTransaction* aTransaction);

  // nsIDOMEventTarget
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

protected:
  ~IDBOpenDBRequest();

  virtual void RootResultValInternal();
  virtual void UnrootResultValInternal();

  // Only touched on the main thread.
  NS_DECL_EVENT_HANDLER(blocked)
  NS_DECL_EVENT_HANDLER(upgradeneeded)
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbrequest_h__
