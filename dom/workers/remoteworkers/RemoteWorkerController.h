/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerController_h
#define mozilla_dom_RemoteWorkerController_h

#include "nsISupportsImpl.h"
#include "nsTArray.h"

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/ServiceWorkerOpArgs.h"
#include "mozilla/dom/ServiceWorkerOpPromise.h"

namespace mozilla {
namespace dom {

/* Here's a graph about this remote workers are spawned.
 *
 *  _________________________________    |   ________________________________
 * |                                 |   |  |                                |
 * |              Parent process     |  IPC |          Creation of Process X |
 * |              PBackground thread |   |  |                                |
 * |                                 |   |  | [RemoteWorkerService::Init()]  |
 * |                                 |   |  |               |                |
 * |                                 |   |  |               | (1)            |
 * | [RemoteWorkerManager::  (2)     |   |  |               V                |
 * |                RegisterActor()]<-------- [new RemoteWorkerServiceChild] |
 * |                                 |   |  |                                |
 * |  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~   |   |  |________________________________|
 * |                                 |   |
 * |  new SharedWorker/ServiceWorker |   |
 * |      |     ^                    |  IPC
 * |  (3) |  (4)|                    |
 * |      V     |                    |   |
 * | [RemoteWorkerController::       |   |
 * |         |         Create(data)] |   |
 * |         | (5)                   |   |
 * |         V                       |   |
 * | [RemoteWorkerManager::Launch()] |   |
 * |         |                       |  IPC   _____________________________
 * |         | (6)                   |   |   |                             |
 * |         |                       |       | Selected content process    |
 * |         V                       |  (7)  |                             |
 * | [SendPRemoteWorkerConstructor()]--------->[new RemoteWorkerChild()]   |
 * |         |                       |   |   |             |               |
 * |         | (8)                   |   |   |             |               |
 * |         V                       |   |   |             V               |
 * | [RemoteWorkerController->       |   |   | RemoteWorkerChild->Exec()   |
 * |         | SetControllerActor()] |   |   |_____________________________|
 * |     (9) |                       |  IPC
 * |         V                       |   |
 * | [RemoteWorkerObserver->         |   |
 * |           CreationCompleted()]  |   |
 * |_________________________________|   |
 *                                       |
 *
 * 1. When a new process starts, it creates a RemoteWorkerService singleton.
 *    This service creates a new thread (Worker Launcher) and from there, it
 *    starts a PBackground RemoteWorkerServiceChild actor.
 * 2. On the parent process, PBackground thread, RemoteWorkerServiceParent
 *    actors are registered into the RemoteWorkerManager service.
 *
 * 3. At some point, a SharedWorker or a ServiceWorker must be executed.
 *    RemoteWorkerController::Create() is used to start the launching. This
 *    method must be called on the parent process, on the PBackground thread.
 * 4. RemoteWorkerController object is immediately returned to the caller. Any
 *    operation done with this controller object will be stored in a queue,
 *    until the launching is correctly executed.
 * 5. RemoteWorkerManager has the list of active RemoteWorkerServiceParent
 *    actors. From them, it picks one.
 *    In case we don't have any content process to select, a new one is
 *    spawned. If this happens, the operation is suspended until a new
 *    RemoteWorkerServiceParent is registered.
 * 6. RemoteWorkerServiceParent is used to create a RemoteWorkerParent.
 * 7. RemoteWorkerChild is created on a selected process and it executes the
 *    WorkerPrivate.
 * 8. The RemoteWorkerParent actor is passed to the RemoteWorkerController.
 * 9. RemoteWorkerController now is ready to continue and it called
 *    RemoteWorkerObserver to inform that the operation is completed.
 *    In case there were pending operations, they are now executed.
 */

class ErrorValue;
class RemoteWorkerControllerParent;
class RemoteWorkerData;
class RemoteWorkerManager;
class RemoteWorkerParent;

class RemoteWorkerObserver {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void CreationFailed() = 0;

  virtual void CreationSucceeded() = 0;

  virtual void ErrorReceived(const ErrorValue& aValue) = 0;

  virtual void Terminated() = 0;
};

class RemoteWorkerController final {
  friend class RemoteWorkerControllerParent;
  friend class RemoteWorkerManager;
  friend class RemoteWorkerParent;

 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteWorkerController)

  static already_AddRefed<RemoteWorkerController> Create(
      const RemoteWorkerData& aData, RemoteWorkerObserver* aObserver,
      base::ProcessId = 0);

  void AddWindowID(uint64_t aWindowID);

  void RemoveWindowID(uint64_t aWindowID);

  void AddPortIdentifier(const MessagePortIdentifier& aPortIdentifier);

  void Terminate();

  void Suspend();

  void Resume();

  void Freeze();

  void Thaw();

 private:
  RemoteWorkerController(const RemoteWorkerData& aData,
                         RemoteWorkerObserver* aObserver);

  ~RemoteWorkerController();

  void SetWorkerActor(RemoteWorkerParent* aActor);

  void NoteDeadWorkerActor();

  void ErrorPropagation(const ErrorValue& aValue);

  void WorkerTerminated();

  void Shutdown();

  void CreationFailed();

  void CreationSucceeded();

  void CancelAllPendingOps();

  template <typename... Args>
  void MaybeStartSharedWorkerOp(Args&&... aArgs);

  void NoteDeadWorker();

  RefPtr<RemoteWorkerObserver> mObserver;
  RefPtr<RemoteWorkerParent> mActor;

  enum {
    ePending,
    eReady,
    eTerminated,
  } mState;

  const bool mIsServiceWorker;

  /**
   * `PendingOp` is responsible for encapsulating logic for starting and
   * canceling pending remote worker operations, as this logic may vary
   * depending on the type of the remote worker and the type of the operation.
   */
  class PendingOp {
   public:
    PendingOp() = default;

    PendingOp(const PendingOp&) = delete;

    PendingOp& operator=(const PendingOp&) = delete;

    virtual ~PendingOp() = default;

    /**
     * Returns `true` if execution has started and `false` otherwise.
     *
     * Starting execution may depend the state of `aOwner.`
     */
    virtual bool MaybeStart(RemoteWorkerController* const aOwner) = 0;

    virtual void Cancel() = 0;
  };

  class PendingSharedWorkerOp final : public PendingOp {
   public:
    enum Type {
      eTerminate,
      eSuspend,
      eResume,
      eFreeze,
      eThaw,
      ePortIdentifier,
      eAddWindowID,
      eRemoveWindowID,
    };

    explicit PendingSharedWorkerOp(Type aType, uint64_t aWindowID = 0);

    explicit PendingSharedWorkerOp(
        const MessagePortIdentifier& aPortIdentifier);

    ~PendingSharedWorkerOp();

    bool MaybeStart(RemoteWorkerController* const aOwner) override;

    void Cancel() override;

   private:
    const Type mType;
    const MessagePortIdentifier mPortIdentifier;
    const uint64_t mWindowID = 0;
    bool mCompleted = false;
  };

  class PendingServiceWorkerOp final : public PendingOp {
   public:
    PendingServiceWorkerOp(ServiceWorkerOpArgs&& aArgs,
                           RefPtr<ServiceWorkerOpPromise::Private> aPromise);

    ~PendingServiceWorkerOp();

    bool MaybeStart(RemoteWorkerController* const aOwner) override;

    void Cancel() override;

   private:
    ServiceWorkerOpArgs mArgs;
    RefPtr<ServiceWorkerOpPromise::Private> mPromise;
  };

  nsTArray<UniquePtr<PendingOp>> mPendingOps;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWorkerController_h
