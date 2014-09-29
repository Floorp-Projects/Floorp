/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbfactory_h__
#define mozilla_dom_indexeddb_idbfactory_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/StorageTypeBinding.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsIPrincipal;
class nsPIDOMWindow;
struct PRThread;

namespace mozilla {

class ErrorResult;

namespace ipc {

class PBackgroundChild;
class PrincipalInfo;

} // namespace ipc

namespace dom {

struct IDBOpenDBOptions;
class TabChild;

namespace indexedDB {

class BackgroundFactoryChild;
class FactoryRequestParams;
class IDBOpenDBRequest;

class IDBFactory MOZ_FINAL
  : public nsISupports
  , public nsWrapperCache
{
  typedef mozilla::dom::StorageType StorageType;
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;
  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;

  class BackgroundCreateCallback;
  struct PendingRequestInfo;

  nsAutoPtr<PrincipalInfo> mPrincipalInfo;

  // If this factory lives on a window then mWindow must be non-null. Otherwise
  // mOwningObject must be non-null.
  nsCOMPtr<nsPIDOMWindow> mWindow;
  JS::Heap<JSObject*> mOwningObject;

  // This will only be set if the factory belongs to a window in a child
  // process.
  nsRefPtr<TabChild> mTabChild;

  nsTArray<nsAutoPtr<PendingRequestInfo>> mPendingRequests;

  BackgroundFactoryChild* mBackgroundActor;

#ifdef DEBUG
  PRThread* mOwningThread;
#endif

  bool mRootedOwningObject;
  bool mBackgroundActorFailed;
  bool mPrivateBrowsingMode;

public:
  static nsresult
  CreateForWindow(nsPIDOMWindow* aWindow,
                  IDBFactory** aFactory);

  static nsresult
  CreateForChromeJS(JSContext* aCx,
                    JS::Handle<JSObject*> aOwningObject,
                    IDBFactory** aFactory);

  static nsresult
  CreateForDatastore(JSContext* aCx,
                    JS::Handle<JSObject*> aOwningObject,
                    IDBFactory** aFactory);

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  void
  SetBackgroundActor(BackgroundFactoryChild* aBackgroundActor);

  void
  ClearBackgroundActor()
  {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  nsPIDOMWindow*
  GetParentObject() const
  {
    return mWindow;
  }

  TabChild*
  GetTabChild() const
  {
    return mTabChild;
  }

  PrincipalInfo*
  GetPrincipalInfo() const
  {
    AssertIsOnOwningThread();

    return mPrincipalInfo;
  }

  already_AddRefed<IDBOpenDBRequest>
  Open(const nsAString& aName,
       uint64_t aVersion,
       ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  Open(const nsAString& aName,
       const IDBOpenDBOptions& aOptions,
       ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  DeleteDatabase(const nsAString& aName,
                 const IDBOpenDBOptions& aOptions,
                 ErrorResult& aRv);

  int16_t
  Cmp(JSContext* aCx,
      JS::Handle<JS::Value> aFirst,
      JS::Handle<JS::Value> aSecond,
      ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  OpenForPrincipal(nsIPrincipal* aPrincipal,
                   const nsAString& aName,
                   uint64_t aVersion,
                   ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  OpenForPrincipal(nsIPrincipal* aPrincipal,
                   const nsAString& aName,
                   const IDBOpenDBOptions& aOptions,
                   ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  DeleteForPrincipal(nsIPrincipal* aPrincipal,
                     const nsAString& aName,
                     const IDBOpenDBOptions& aOptions,
                     ErrorResult& aRv);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBFactory)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

private:
  IDBFactory();
  ~IDBFactory();

  static nsresult
  CreateForJSInternal(JSContext* aCx,
                      JS::Handle<JSObject*> aOwningObject,
                      nsAutoPtr<PrincipalInfo>& aPrincipalInfo,
                      IDBFactory** aFactory);

  already_AddRefed<IDBOpenDBRequest>
  OpenInternal(nsIPrincipal* aPrincipal,
               const nsAString& aName,
               const Optional<uint64_t>& aVersion,
               const Optional<StorageType>& aStorageType,
               bool aDeleting,
               ErrorResult& aRv);

  nsresult
  BackgroundActorCreated(PBackgroundChild* aBackgroundActor);

  void
  BackgroundActorFailed();

  nsresult
  InitiateRequest(IDBOpenDBRequest* aRequest,
                  const FactoryRequestParams& aParams);
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbfactory_h__
