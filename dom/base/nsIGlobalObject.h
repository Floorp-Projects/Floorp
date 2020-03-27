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
#define NS_IGLOBALOBJECT_IID                         \
  {                                                  \
    0x11afa8be, 0xd997, 0x4e07, {                    \
      0xa6, 0xa3, 0x6f, 0x87, 0x2e, 0xc3, 0xee, 0x7f \
    }                                                \
  }

class nsCycleCollectionTraversalCallback;
class nsIPrincipal;
class nsPIDOMWindowInner;

namespace mozilla {
class DOMEventTargetHelper;
namespace dom {
class VoidFunction;
class DebuggerNotificationManager;
class ServiceWorker;
class ServiceWorkerRegistration;
class ServiceWorkerRegistrationDescriptor;
}  // namespace dom
}  // namespace mozilla

class nsIGlobalObject : public nsISupports,
                        public mozilla::dom::DispatcherTrait {
  nsTArray<nsCString> mHostObjectURIs;

  // Raw pointers to bound DETH objects.  These are added by
  // AddEventTargetObject().
  mozilla::LinkedList<mozilla::DOMEventTargetHelper> mEventTargetObjects;

  bool mIsDying;
  bool mIsScriptForbidden;

 protected:
  bool mIsInnerWindow;

  nsIGlobalObject()
      : mIsDying(false), mIsScriptForbidden(false), mIsInnerWindow(false) {}

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
  bool IsDying() const { return mIsDying; }

  /**
   * Is it currently forbidden to call into script?  JS-implemented WebIDL is
   * a special case that's always allowed because it has the system principal,
   * and callers should indicate this.
   */
  bool IsScriptForbidden(JSObject* aCallback,
                         bool aIsJSImplementedWebIDL = false) const;

  /**
   * Return the JSObject for this global, if it still has one.  Otherwise return
   * null.
   *
   * If non-null is returned, then the returned object will have been already
   * exposed to active JS, so callers do not need to do it.
   */
  virtual JSObject* GetGlobalJSObject() = 0;

  /**
   * Return the JSObject for this global _without_ exposing it to active JS.
   * This may return a gray object.
   *
   * This method is appropriate to use in assertions (so there is less of a
   * difference in GC/CC marking between debug and optimized builds) and in
   * situations where we are sure no CC activity can happen while the return
   * value is used and the return value does not end up escaping to the heap in
   * any way.  In all other cases, and in particular in cases where the return
   * value is held in a JS::Rooted or passed to the JSAutoRealm constructor, use
   * GetGlobalJSObject.
   */
  virtual JSObject* GetGlobalJSObjectPreserveColor() const = 0;

  /**
   * Check whether this nsIGlobalObject still has a JSObject associated with it,
   * or whether it's torn-down enough that the JSObject is gone.
   */
  bool HasJSGlobal() const { return GetGlobalJSObjectPreserveColor(); }

  // This method is not meant to be overridden.
  nsIPrincipal* PrincipalOrNull();

  void RegisterHostObjectURI(const nsACString& aURI);

  void UnregisterHostObjectURI(const nsACString& aURI);

  // Any CC class inheriting nsIGlobalObject should call these 2 methods if it
  // exposes the URL API.
  void UnlinkHostObjectURIs();
  void TraverseHostObjectURIs(nsCycleCollectionTraversalCallback& aCb);

  // DETH objects must register themselves on the global when they
  // bind to it in order to get the DisconnectFromOwner() method
  // called correctly.  RemoveEventTargetObject() must be called
  // before the DETH object is destroyed.
  void AddEventTargetObject(mozilla::DOMEventTargetHelper* aObject);
  void RemoveEventTargetObject(mozilla::DOMEventTargetHelper* aObject);

  // Iterate the registered DETH objects and call the given function
  // for each one.
  void ForEachEventTargetObject(
      const std::function<void(mozilla::DOMEventTargetHelper*, bool* aDoneOut)>&
          aFunc) const;

  virtual bool IsInSyncOperation() { return false; }

  virtual mozilla::dom::DebuggerNotificationManager*
  GetOrCreateDebuggerNotificationManager() {
    return nullptr;
  }

  virtual mozilla::dom::DebuggerNotificationManager*
  GetExistingDebuggerNotificationManager() {
    return nullptr;
  }

  virtual mozilla::Maybe<mozilla::dom::ClientInfo> GetClientInfo() const;

  virtual mozilla::Maybe<nsID> GetAgentClusterId() const;

  virtual bool IsSharedMemoryAllowed() const { return false; }

  virtual mozilla::Maybe<mozilla::dom::ServiceWorkerDescriptor> GetController()
      const;

  // Get the DOM object for the given descriptor or attempt to create one.
  // Creation can still fail and return nullptr during shutdown, etc.
  virtual RefPtr<mozilla::dom::ServiceWorker> GetOrCreateServiceWorker(
      const mozilla::dom::ServiceWorkerDescriptor& aDescriptor);

  // Get the DOM object for the given descriptor or return nullptr if it does
  // not exist.
  virtual RefPtr<mozilla::dom::ServiceWorkerRegistration>
  GetServiceWorkerRegistration(
      const mozilla::dom::ServiceWorkerRegistrationDescriptor& aDescriptor)
      const;

  // Get the DOM object for the given descriptor or attempt to create one.
  // Creation can still fail and return nullptr during shutdown, etc.
  virtual RefPtr<mozilla::dom::ServiceWorkerRegistration>
  GetOrCreateServiceWorkerRegistration(
      const mozilla::dom::ServiceWorkerRegistrationDescriptor& aDescriptor);

  // Returns a pointer to this object as an inner window if this is one or
  // nullptr otherwise.
  nsPIDOMWindowInner* AsInnerWindow();

  void QueueMicrotask(mozilla::dom::VoidFunction& aCallback);

 protected:
  virtual ~nsIGlobalObject();

  void StartDying() { mIsDying = true; }

  void StartForbiddingScript() { mIsScriptForbidden = true; }
  void StopForbiddingScript() { mIsScriptForbidden = false; }

  void DisconnectEventTargetObjects();

  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aSizeOf) const;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIGlobalObject, NS_IGLOBALOBJECT_IID)

#endif  // nsIGlobalObject_h__
