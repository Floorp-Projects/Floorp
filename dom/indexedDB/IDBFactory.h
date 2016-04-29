/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbfactory_h__
#define mozilla_dom_idbfactory_h__

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
class nsPIDOMWindowInner;
struct PRThread;

namespace mozilla {

class ErrorResult;

namespace ipc {

class PBackgroundChild;
class PrincipalInfo;

} // namespace ipc

namespace dom {

struct IDBOpenDBOptions;
class IDBOpenDBRequest;
template <typename> class Optional;
class TabChild;

namespace indexedDB {
class BackgroundFactoryChild;
class FactoryRequestParams;
class LoggingInfo;
}

class IDBFactory final
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
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  JS::Heap<JSObject*> mOwningObject;

  // This will only be set if the factory belongs to a window in a child
  // process.
  RefPtr<TabChild> mTabChild;

  nsTArray<nsAutoPtr<PendingRequestInfo>> mPendingRequests;

  indexedDB::BackgroundFactoryChild* mBackgroundActor;

#ifdef DEBUG
  PRThread* mOwningThread;
#endif

  uint64_t mInnerWindowID;

  bool mBackgroundActorFailed;
  bool mPrivateBrowsingMode;

public:
  static nsresult
  CreateForWindow(nsPIDOMWindowInner* aWindow,
                  IDBFactory** aFactory);

  static nsresult
  CreateForMainThreadJS(JSContext* aCx,
                        JS::Handle<JSObject*> aOwningObject,
                        IDBFactory** aFactory);

  static nsresult
  CreateForWorker(JSContext* aCx,
                  JS::Handle<JSObject*> aOwningObject,
                  const PrincipalInfo& aPrincipalInfo,
                  uint64_t aInnerWindowID,
                  IDBFactory** aFactory);

  static bool
  AllowedForWindow(nsPIDOMWindowInner* aWindow);

  static bool
  AllowedForPrincipal(nsIPrincipal* aPrincipal,
                      bool* aIsSystemPrincipal = nullptr);

#ifdef DEBUG
  void
  AssertIsOnOwningThread() const;

  PRThread*
  OwningThread() const;
#else
  void
  AssertIsOnOwningThread() const
  { }
#endif

  void
  ClearBackgroundActor()
  {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  void
  IncrementParentLoggingRequestSerialNumber();

  nsPIDOMWindowInner*
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

  uint64_t
  InnerWindowID() const
  {
    AssertIsOnOwningThread();

    return mInnerWindowID;
  }

  bool
  IsChrome() const;

  already_AddRefed<IDBOpenDBRequest>
  Open(JSContext* aCx,
       const nsAString& aName,
       uint64_t aVersion,
       ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  Open(JSContext* aCx,
       const nsAString& aName,
       const IDBOpenDBOptions& aOptions,
       ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  DeleteDatabase(JSContext* aCx,
                 const nsAString& aName,
                 const IDBOpenDBOptions& aOptions,
                 ErrorResult& aRv);

  int16_t
  Cmp(JSContext* aCx,
      JS::Handle<JS::Value> aFirst,
      JS::Handle<JS::Value> aSecond,
      ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  OpenForPrincipal(JSContext* aCx,
                   nsIPrincipal* aPrincipal,
                   const nsAString& aName,
                   uint64_t aVersion,
                   ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  OpenForPrincipal(JSContext* aCx,
                   nsIPrincipal* aPrincipal,
                   const nsAString& aName,
                   const IDBOpenDBOptions& aOptions,
                   ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  DeleteForPrincipal(JSContext* aCx,
                     nsIPrincipal* aPrincipal,
                     const nsAString& aName,
                     const IDBOpenDBOptions& aOptions,
                     ErrorResult& aRv);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBFactory)

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  IDBFactory();
  ~IDBFactory();

  static nsresult
  CreateForMainThreadJSInternal(JSContext* aCx,
                                JS::Handle<JSObject*> aOwningObject,
                                nsAutoPtr<PrincipalInfo>& aPrincipalInfo,
                                IDBFactory** aFactory);

  static nsresult
  CreateForJSInternal(JSContext* aCx,
                      JS::Handle<JSObject*> aOwningObject,
                      nsAutoPtr<PrincipalInfo>& aPrincipalInfo,
                      uint64_t aInnerWindowID,
                      IDBFactory** aFactory);

  static nsresult
  AllowedForWindowInternal(nsPIDOMWindowInner* aWindow,
                           nsIPrincipal** aPrincipal);

  already_AddRefed<IDBOpenDBRequest>
  OpenInternal(JSContext* aCx,
               nsIPrincipal* aPrincipal,
               const nsAString& aName,
               const Optional<uint64_t>& aVersion,
               const Optional<StorageType>& aStorageType,
               bool aDeleting,
               ErrorResult& aRv);

  nsresult
  BackgroundActorCreated(PBackgroundChild* aBackgroundActor,
                         const indexedDB::LoggingInfo& aLoggingInfo);

  void
  BackgroundActorFailed();

  nsresult
  InitiateRequest(IDBOpenDBRequest* aRequest,
                  const indexedDB::FactoryRequestParams& aParams);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_idbfactory_h__
