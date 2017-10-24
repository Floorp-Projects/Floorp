/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorage_h
#define mozilla_dom_cache_CacheStorage_h

#include "mozilla/dom/cache/Types.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace ipc {
  class PrincipalInfo;
} // namespace ipc

namespace dom {

enum class CacheStorageNamespace : uint8_t;
class Promise;

namespace workers {
  class WorkerPrivate;
} // namespace workers

namespace cache {

class CacheStorageChild;
class CacheWorkerHolder;

class CacheStorage final : public nsISupports
                         , public nsWrapperCache
                         , public TypeUtils
{
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;

public:
  static already_AddRefed<CacheStorage>
  CreateOnMainThread(Namespace aNamespace, nsIGlobalObject* aGlobal,
                     nsIPrincipal* aPrincipal, bool aStorageDisabled,
                     bool aForceTrustedOrigin, ErrorResult& aRv);

  static already_AddRefed<CacheStorage>
  CreateOnWorker(Namespace aNamespace, nsIGlobalObject* aGlobal,
                 workers::WorkerPrivate* aWorkerPrivate, ErrorResult& aRv);

  static bool
  DefineCaches(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

  // webidl interface methods
  already_AddRefed<Promise>
  Match(JSContext* aCx, const RequestOrUSVString& aRequest,
        const CacheQueryOptions& aOptions, ErrorResult& aRv);
  already_AddRefed<Promise> Has(const nsAString& aKey, ErrorResult& aRv);
  already_AddRefed<Promise> Open(const nsAString& aKey, ErrorResult& aRv);
  already_AddRefed<Promise> Delete(const nsAString& aKey, ErrorResult& aRv);
  already_AddRefed<Promise> Keys(ErrorResult& aRv);

  // chrome-only webidl interface methods
  static already_AddRefed<CacheStorage>
  Constructor(const GlobalObject& aGlobal, CacheStorageNamespace aNamespace,
              nsIPrincipal* aPrincipal, ErrorResult& aRv);

  // binding methods
  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  nsISupports* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aContext, JS::Handle<JSObject*> aGivenProto) override;

  // Called when CacheStorageChild actor is being destroyed
  void DestroyInternal(CacheStorageChild* aActor);

  // TypeUtils methods
  virtual nsIGlobalObject* GetGlobalObject() const override;
#ifdef DEBUG
  virtual void AssertOwningThread() const override;
#endif

  virtual mozilla::ipc::PBackgroundChild*
  GetIPCManager() override;

private:
  CacheStorage(Namespace aNamespace, nsIGlobalObject* aGlobal,
               const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
               CacheWorkerHolder* aWorkerHolder);
  explicit CacheStorage(nsresult aFailureResult);
  ~CacheStorage();

  struct Entry;
  void RunRequest(nsAutoPtr<Entry>&& aEntry);

  OpenMode
  GetOpenMode() const;

  const Namespace mNamespace;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  UniquePtr<mozilla::ipc::PrincipalInfo> mPrincipalInfo;

  // weak ref cleared in DestroyInternal
  CacheStorageChild* mActor;

  nsresult mStatus;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CacheStorage)
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorage_h
