/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIGlobalObject_h__
#define nsIGlobalObject_h__

#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/DispatcherTrait.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "nsHashKeys.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "js/TypeDecls.h"

// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_IGLOBALOBJECT_IID \
{ 0x11afa8be, 0xd997, 0x4e07, \
{ 0xa6, 0xa3, 0x6f, 0x87, 0x2e, 0xc3, 0xee, 0x7f } }

class nsCycleCollectionTraversalCallback;
class nsIPrincipal;

namespace mozilla {
class DOMEventTargetHelper;
namespace dom {
class ServiceWorker;
class ServiceWorkerRegistration;
class ServiceWorkerRegistrationDescriptor;
} // namespace dom
} // namespace mozilla

class nsIGlobalObject : public nsISupports,
                        public mozilla::dom::DispatcherTrait
{
  nsTArray<nsCString> mHostObjectURIs;

  // Raw pointers to bound DETH objects.  These are added by
  // AddEventTargetObject().
  mozilla::LinkedList<mozilla::DOMEventTargetHelper> mEventTargetObjects;

  bool mIsDying;

protected:
  nsIGlobalObject()
   : mIsDying(false)
  {}

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IGLOBALOBJECT_IID)

  /**
   * This check is added to deal with Promise microtask queues. On the main
   * thread, we do not impose restrictions about when script stops running or
   * when runnables can no longer be dispatched to the main thread. This means
   * it is possible for a Promise chain to keep resolving an infinite chain of
   * promises, preventing the browser from shutting down. See Bug 1058695. To
   * prevent this, the nsGlobalWindow subclass sets this flag when it is
   * closed. The Promise implementation checks this and prohibits new runnables
   * from being dispatched.
   *
   * We pair this with checks during processing the promise microtask queue
   * that pops up the slow script dialog when the Promise queue is preventing
   * a window from going away.
   */
  bool
  IsDying() const
  {
    return mIsDying;
  }

  // GetGlobalJSObject may return a gray object.  If this ever changes so that
  // it stops doing that, please simplify the code in FindAssociatedGlobal in
  // BindingUtils.h that does JS::ExposeObjectToActiveJS on the return value of
  // GetGlobalJSObject.  Also, in that case the JS::ExposeObjectToActiveJS in
  // AutoJSAPI::InitInternal can probably be removed.  And also the similar
  // calls in XrayWrapper and nsGlobalWindow.
  virtual JSObject* GetGlobalJSObject() = 0;

  // This method is not meant to be overridden.
  nsIPrincipal* PrincipalOrNull();

  void RegisterHostObjectURI(const nsACString& aURI);

  void UnregisterHostObjectURI(const nsACString& aURI);

  // Any CC class inheriting nsIGlobalObject should call these 2 methods if it
  // exposes the URL API.
  void UnlinkHostObjectURIs();
  void TraverseHostObjectURIs(nsCycleCollectionTraversalCallback &aCb);

  // DETH objects must register themselves on the global when they
  // bind to it in order to get the DisconnectFromOwner() method
  // called correctly.  RemoveEventTargetObject() must be called
  // before the DETH object is destroyed.
  void AddEventTargetObject(mozilla::DOMEventTargetHelper* aObject);
  void RemoveEventTargetObject(mozilla::DOMEventTargetHelper* aObject);

  // Iterate the registered DETH objects and call the given function
  // for each one.
  void
  ForEachEventTargetObject(const std::function<void(mozilla::DOMEventTargetHelper*, bool* aDoneOut)>& aFunc) const;

  virtual bool IsInSyncOperation() { return false; }

  virtual mozilla::Maybe<mozilla::dom::ClientInfo>
  GetClientInfo() const;

  virtual mozilla::Maybe<mozilla::dom::ServiceWorkerDescriptor>
  GetController() const;

  // Get the DOM object for the given descriptor or attempt to create one.
  // Creation can still fail and return nullptr during shutdown, etc.
  virtual RefPtr<mozilla::dom::ServiceWorker>
  GetOrCreateServiceWorker(const mozilla::dom::ServiceWorkerDescriptor& aDescriptor);

  // Get the DOM object for the given descriptor or attempt to create one.
  // Creation can still fail and return nullptr during shutdown, etc.
  virtual RefPtr<mozilla::dom::ServiceWorkerRegistration>
  GetOrCreateServiceWorkerRegistration(const mozilla::dom::ServiceWorkerRegistrationDescriptor& aDescriptor);

protected:
  virtual ~nsIGlobalObject();

  void
  StartDying()
  {
    mIsDying = true;
  }

  void
  DisconnectEventTargetObjects();

  size_t
  ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aSizeOf) const;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIGlobalObject,
                              NS_IGLOBALOBJECT_IID)

#endif // nsIGlobalObject_h__
