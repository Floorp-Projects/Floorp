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

}  // namespace ipc

namespace dom {

class LSDatabase;
class LSObjectChild;
class LSObserver;
class LSRequestChild;
class LSRequestChildCallback;
class LSRequestParams;
class LSRequestResponse;

/**
 * Backs the WebIDL `Storage` binding; all content LocalStorage calls are
 * handled by this class.
 *
 * ## Semantics under e10s / multi-process ##
 *
 * A snapshot mechanism used in conjuction with stable points ensures that JS
 * run-to-completion semantics are experienced even if the same origin is
 * concurrently accessing LocalStorage across multiple content processes.
 *
 * ### Snapshot Consistency ###
 *
 * An LSSnapshot is created locally whenever the contents of LocalStorage are
 * about to be read or written (including length).  This synchronously
 * establishes a corresponding Snapshot in PBackground in the parent process.
 * An effort is made to send as much data from the parent process as possible,
 * so sites using a small/reasonable amount of LocalStorage data will have it
 * sent to the content process for immediate access.  Sites with greater
 * LocalStorage usage may only have some of the information relayed.  In that
 * case, the parent Snapshot will ensure that it retains the exact state of the
 * parent Datastore at the moment the Snapshot was created.
 */
class LSObject final : public Storage {
  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;

  friend nsGlobalWindowInner;

  nsAutoPtr<PrincipalInfo> mPrincipalInfo;
  nsAutoPtr<PrincipalInfo> mStoragePrincipalInfo;

  RefPtr<LSDatabase> mDatabase;
  RefPtr<LSObserver> mObserver;

  uint32_t mPrivateBrowsingId;
  Maybe<nsID> mClientId;
  nsCString mOrigin;
  nsCString mOriginKey;
  nsString mDocumentURI;

  bool mInExplicitSnapshot;

 public:
  static void Initialize();

  /**
   * The normal creation path invoked by nsGlobalWindowInner.
   */
  static nsresult CreateForWindow(nsPIDOMWindowInner* aWindow,
                                  Storage** aStorage);

  /**
   * nsIDOMStorageManager creation path for use in testing logic.  Supports the
   * system principal where CreateForWindow does not.  This is also why aPrivate
   * exists separate from the principal; because the system principal can never
   * be mutated to have a private browsing id even though it can be used in a
   * window/document marked as private browsing.  That's a legacy issue that is
   * being dealt with, but it's why it exists here.
   */
  static nsresult CreateForPrincipal(nsPIDOMWindowInner* aWindow,
                                     nsIPrincipal* aPrincipal,
                                     nsIPrincipal* aStoragePrincipal,
                                     const nsAString& aDocumentURI,
                                     bool aPrivate, LSObject** aObject);

  /**
   * Used for requests from the parent process to the parent process; in that
   * case we want ActorsParent to know our event-target and this is better than
   * trying to tunnel the pointer through IPC.
   */
  static already_AddRefed<nsISerialEventTarget> GetSyncLoopEventTarget();

  /**
   * Helper invoked by ContentChild::OnChannelReceivedMessage when a sync IPC
   * message is received.  This will be invoked on the IPC I/O thread and it
   * will set the gPendingSyncMessage flag to true.  It will also force the sync
   * loop (if it's active) to check the gPendingSyncMessage flag which will
   * result in premature finish of the loop.
   *
   * This is necessary to unblock the main thread when a sync IPC message is
   * received to avoid the potential for browser deadlock.  This should only
   * occur in (ugly) testing scenarios where CPOWs are in use.
   *
   * Aborted sync loop will result in the underlying LSRequest being explicitly
   * canceled, resulting in the parent sending an NS_ERROR_FAILURE result.
   */
  static void OnSyncMessageReceived();

  /*
   * Helper invoked by ContentChild::OnMessageReceived when a sync IPC message
   * has been handled.  This will be invoked on the main thread and it will
   * set the gPendingSyncMessage flag to false.
   */
  static void OnSyncMessageHandled();

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(LSObject); }

  const nsString& DocumentURI() const { return mDocumentURI; }

  LSRequestChild* StartRequest(nsIEventTarget* aMainEventTarget,
                               const LSRequestParams& aParams,
                               LSRequestChildCallback* aCallback);

  // Storage overrides.
  StorageType Type() const override;

  bool IsForkOf(const Storage* aStorage) const override;

  int64_t GetOriginQuotaUsage() const override;

  uint32_t GetLength(nsIPrincipal& aSubjectPrincipal,
                     ErrorResult& aError) override;

  void Key(uint32_t aIndex, nsAString& aResult, nsIPrincipal& aSubjectPrincipal,
           ErrorResult& aError) override;

  void GetItem(const nsAString& aKey, nsAString& aResult,
               nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) override;

  void GetSupportedNames(nsTArray<nsString>& aNames) override;

  void SetItem(const nsAString& aKey, const nsAString& aValue,
               nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) override;

  void RemoveItem(const nsAString& aKey, nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aError) override;

  void Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) override;

  //////////////////////////////////////////////////////////////////////////////
  // Testing Methods: See Storage.h
  void Open(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) override;

  void Close(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) override;

  void BeginExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aError) override;

  void EndExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                           ErrorResult& aError) override;

  //////////////////////////////////////////////////////////////////////////////

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(LSObject, Storage)

 private:
  LSObject(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
           nsIPrincipal* aStoragePrincipal);

  ~LSObject();

  nsresult DoRequestSynchronously(const LSRequestParams& aParams,
                                  LSRequestResponse& aResponse);

  nsresult EnsureDatabase();

  void DropDatabase();

  /**
   * Invoked by nsGlobalWindowInner whenever a new "storage" event listener is
   * added to the window in order to ensure that "storage" events are received
   * from other processes.  (`LSObject::OnChange` directly invokes
   * `Storage::NotifyChange` to notify in-process listeners.)
   *
   * If this is the first request in the process for an observer for this
   * origin, this will trigger a RequestHelper-mediated synchronous LSRequest
   * to prepare a new observer in the parent process and also construction of
   * corresponding actors, which will result in the observer being fully
   * registered in the parent process.
   */
  nsresult EnsureObserver();

  /**
   * Invoked by nsGlobalWindowInner whenever its last "storage" event listener
   * is removed.
   */
  void DropObserver();

  /**
   * Internal helper method used by mutation methods that wraps the call to
   * Storage::NotifyChange to generate same-process "storage" events.
   */
  void OnChange(const nsAString& aKey, const nsAString& aOldValue,
                const nsAString& aNewValue);

  nsresult EndExplicitSnapshotInternal();

  // Storage overrides.
  void LastRelease() override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_LSObject_h
