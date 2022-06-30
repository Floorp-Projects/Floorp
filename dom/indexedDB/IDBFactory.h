/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbfactory_h__
#define mozilla_dom_idbfactory_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StorageTypeBinding.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;
class nsIPrincipal;
class nsISerialEventTarget;
class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace ipc {

class PBackgroundChild;
class PrincipalInfo;

}  // namespace ipc

namespace dom {

struct IDBOpenDBOptions;
class IDBOpenDBRequest;
template <typename>
class Optional;
class BrowserChild;
enum class CallerType : uint32_t;

namespace indexedDB {
class BackgroundFactoryChild;
class FactoryRequestParams;
class LoggingInfo;
}  // namespace indexedDB

class IDBFactory final : public nsISupports, public nsWrapperCache {
  using StorageType = mozilla::dom::StorageType;
  using PBackgroundChild = mozilla::ipc::PBackgroundChild;
  using PrincipalInfo = mozilla::ipc::PrincipalInfo;

  class BackgroundCreateCallback;
  struct PendingRequestInfo;
  struct IDBFactoryGuard {};

  UniquePtr<PrincipalInfo> mPrincipalInfo;

  nsCOMPtr<nsIGlobalObject> mGlobal;

  // This will only be set if the factory belongs to a window in a child
  // process.
  RefPtr<BrowserChild> mBrowserChild;

  indexedDB::BackgroundFactoryChild* mBackgroundActor;

  // It is either set to a DocGroup-specific EventTarget if created by
  // CreateForWindow() or set to GetCurrentSerialEventTarget() otherwise.
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  uint64_t mInnerWindowID;
  uint32_t mActiveTransactionCount;
  uint32_t mActiveDatabaseCount;

  bool mBackgroundActorFailed;
  bool mPrivateBrowsingMode;

 public:
  explicit IDBFactory(const IDBFactoryGuard&);

  static Result<RefPtr<IDBFactory>, nsresult> CreateForWindow(
      nsPIDOMWindowInner* aWindow);

  static Result<RefPtr<IDBFactory>, nsresult> CreateForMainThreadJS(
      nsIGlobalObject* aGlobal);

  static Result<RefPtr<IDBFactory>, nsresult> CreateForWorker(
      nsIGlobalObject* aGlobal, const PrincipalInfo& aPrincipalInfo,
      uint64_t aInnerWindowID);

  static bool AllowedForWindow(nsPIDOMWindowInner* aWindow);

  static bool AllowedForPrincipal(nsIPrincipal* aPrincipal,
                                  bool* aIsSystemPrincipal = nullptr);

  static bool IsEnabled(JSContext* aCx, JSObject* aGlobal);

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(IDBFactory); }

  nsISerialEventTarget* EventTarget() const {
    AssertIsOnOwningThread();
    MOZ_RELEASE_ASSERT(mEventTarget);
    return mEventTarget;
  }

  void ClearBackgroundActor() {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  // Increase/Decrease the number of active transactions for the decision
  // making of preemption and throttling.
  // Note: If the state of its actor is not committed or aborted, it could block
  // IDB operations in other window.
  void UpdateActiveTransactionCount(int32_t aDelta);

  // Increase/Decrease the number of active databases and IDBOpenRequests for
  // the decision making of preemption and throttling.
  // Note: A non-closed database or a pending IDBOpenRequest could block
  // IDB operations in other window.
  void UpdateActiveDatabaseCount(int32_t aDelta);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  BrowserChild* GetBrowserChild() const { return mBrowserChild; }

  PrincipalInfo* GetPrincipalInfo() const {
    AssertIsOnOwningThread();

    return mPrincipalInfo.get();
  }

  uint64_t InnerWindowID() const {
    AssertIsOnOwningThread();

    return mInnerWindowID;
  }

  bool IsChrome() const;

  [[nodiscard]] RefPtr<IDBOpenDBRequest> Open(JSContext* aCx,
                                              const nsAString& aName,
                                              uint64_t aVersion,
                                              CallerType aCallerType,
                                              ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBOpenDBRequest> Open(JSContext* aCx,
                                              const nsAString& aName,
                                              const IDBOpenDBOptions& aOptions,
                                              CallerType aCallerType,
                                              ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBOpenDBRequest> DeleteDatabase(
      JSContext* aCx, const nsAString& aName, const IDBOpenDBOptions& aOptions,
      CallerType aCallerType, ErrorResult& aRv);

  int16_t Cmp(JSContext* aCx, JS::Handle<JS::Value> aFirst,
              JS::Handle<JS::Value> aSecond, ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBOpenDBRequest> OpenForPrincipal(
      JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
      uint64_t aVersion, SystemCallerGuarantee, ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBOpenDBRequest> OpenForPrincipal(
      JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
      const IDBOpenDBOptions& aOptions, SystemCallerGuarantee,
      ErrorResult& aRv);

  [[nodiscard]] RefPtr<IDBOpenDBRequest> DeleteForPrincipal(
      JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
      const IDBOpenDBOptions& aOptions, SystemCallerGuarantee,
      ErrorResult& aRv);

  void DisconnectFromGlobal(nsIGlobalObject* aOldGlobal);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBFactory)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~IDBFactory();

  static Result<RefPtr<IDBFactory>, nsresult> CreateForMainThreadJSInternal(
      nsIGlobalObject* aGlobal, UniquePtr<PrincipalInfo> aPrincipalInfo);

  static Result<RefPtr<IDBFactory>, nsresult> CreateInternal(
      nsIGlobalObject* aGlobal, UniquePtr<PrincipalInfo> aPrincipalInfo,
      uint64_t aInnerWindowID);

  static nsresult AllowedForWindowInternal(nsPIDOMWindowInner* aWindow,
                                           nsCOMPtr<nsIPrincipal>* aPrincipal);

  [[nodiscard]] RefPtr<IDBOpenDBRequest> OpenInternal(
      JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
      const Optional<uint64_t>& aVersion,
      const Optional<StorageType>& aStorageType, bool aDeleting,
      CallerType aCallerType, ErrorResult& aRv);

  nsresult InitiateRequest(const NotNull<RefPtr<IDBOpenDBRequest>>& aRequest,
                           const indexedDB::FactoryRequestParams& aParams);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbfactory_h__
