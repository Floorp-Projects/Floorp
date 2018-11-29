/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSObject_h
#define mozilla_dom_localstorage_LSObject_h

#include "mozilla/dom/Storage.h"

class nsGlobalWindowInner;
class nsIPrincipal;
class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace ipc {

class PrincipalInfo;

} // namespace ipc

namespace dom {

class LSDatabase;
class LSNotifyInfo;
class LSObjectChild;
class LSObserver;
class LSRequestChild;
class LSRequestChildCallback;
class LSRequestParams;
class LSRequestResponse;
class LSWriteOpResponse;

class LSObject final
  : public Storage
{
  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;

  friend nsGlobalWindowInner;

  nsAutoPtr<PrincipalInfo> mPrincipalInfo;

  RefPtr<LSDatabase> mDatabase;
  RefPtr<LSObserver> mObserver;

  LSObjectChild* mActor;

  uint32_t mPrivateBrowsingId;
  nsCString mOrigin;
  nsString mDocumentURI;
  bool mActorFailed;

public:
  static nsresult
  CreateForWindow(nsPIDOMWindowInner* aWindow,
                  Storage** aStorage);

  static nsresult
  CreateForPrincipal(nsPIDOMWindowInner* aWindow,
                     nsIPrincipal* aPrincipal,
                     const nsAString& aDocumentURI,
                     bool aPrivate,
                     LSObject** aObject);

  /**
   * Used for requests from the parent process to the parent process; in that
   * case we want ActorsParent to know our event-target and this is better than
   * trying to tunnel the pointer through IPC.
   */
  static already_AddRefed<nsIEventTarget>
  GetSyncLoopEventTarget();

  static void
  CancelSyncLoop();

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(LSObject);
  }

  void
  ClearActor()
  {
    AssertIsOnOwningThread();

    mActor = nullptr;
  }

  LSRequestChild*
  StartRequest(nsIEventTarget* aMainEventTarget,
               const LSRequestParams& aParams,
               LSRequestChildCallback* aCallback);

  // Storage overrides.
  StorageType
  Type() const override;

  bool
  IsForkOf(const Storage* aStorage) const override;

  int64_t
  GetOriginQuotaUsage() const override;

  uint32_t
  GetLength(nsIPrincipal& aSubjectPrincipal,
            ErrorResult& aError) override;

  void
  Key(uint32_t aIndex,
      nsAString& aResult,
      nsIPrincipal& aSubjectPrincipal,
      ErrorResult& aError) override;

  void
  GetItem(const nsAString& aKey,
          nsAString& aResult,
          nsIPrincipal& aSubjectPrincipal,
          ErrorResult& aError) override;

  void
  GetSupportedNames(nsTArray<nsString>& aNames) override;

  void
  SetItem(const nsAString& aKey,
          const nsAString& aValue,
          nsIPrincipal& aSubjectPrincipal,
          ErrorResult& aError) override;

  void
  RemoveItem(const nsAString& aKey,
             nsIPrincipal& aSubjectPrincipal,
             ErrorResult& aError) override;

  void
  Clear(nsIPrincipal& aSubjectPrincipal,
        ErrorResult& aError) override;

  void
  Open(nsIPrincipal& aSubjectPrincipal,
       ErrorResult& aError) override;

  void
  Close(nsIPrincipal& aSubjectPrincipal,
        ErrorResult& aError) override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(LSObject, Storage)

private:
  LSObject(nsPIDOMWindowInner* aWindow,
           nsIPrincipal* aPrincipal);

  ~LSObject();

  nsresult
  DoRequestSynchronously(const LSRequestParams& aParams,
                         LSRequestResponse& aResponse);

  nsresult
  EnsureDatabase();

  void
  DropDatabase();

  nsresult
  EnsureObserver();

  void
  DropObserver();

  nsresult
  GetInfoFromResponse(const LSWriteOpResponse& aResponse,
                      LSNotifyInfo& aInfo);

  void
  OnChange(const nsAString& aKey,
           const nsAString& aOldValue,
           const nsAString& aNewValue);

  // Storage overrides.
  void
  LastRelease() override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_localstorage_LSObject_h
