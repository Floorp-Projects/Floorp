/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerRef_h
#define mozilla_dom_workers_WorkerRef_h

#include "mozilla/dom/WorkerStatus.h"
#include "mozilla/MoveOnlyFunction.h"
#include "mozilla/RefPtr.h"
#include "nsISupports.h"

namespace mozilla::dom {

/*
 * If you want to play with a DOM Worker, you must know that it can go away
 * at any time if nothing prevents its shutting down. This documentation helps
 * to understand how to play with DOM Workers correctly.
 *
 * There are several reasons why a DOM Worker could go away. Here is the
 * complete list:
 *
 * a. GC/CC - If the DOM Worker thread is idle and the Worker object is garbage
 *    collected, it goes away.
 * b. The worker script can call self.close()
 * c. The Worker object calls worker.terminate()
 * d. Firefox is shutting down.
 *
 * When a DOM Worker goes away, it does several steps. See more in
 * WorkerStatus.h. The DOM Worker thread will basically stop scheduling
 * WorkerRunnables, and eventually WorkerControlRunnables. But if there is
 * something preventing the shutting down, it will always possible to dispatch
 * WorkerControlRunnables.  Of course, at some point, the worker _must_ be
 * released, otherwise firefox will leak it and the browser shutdown will hang.
 *
 * WeakWorkerRef is a refcounted, NON thread-safe object.
 *
 * From this object, you can obtain a WorkerPrivate, calling
 * WeakWorkerRef::GetPrivate(). It returns nullptr if the worker is shutting
 * down or if it is already gone away.
 *
 * If you want to know when a DOM Worker starts the shutting down procedure,
 * pass a callback to the mozilla::dom::WeakWorkerRef::Create() method.
 * Your function will be called. Note that _after_ the callback,
 * WeakWorkerRef::GetPrivate() will return nullptr.
 *
 * How to keep a DOM Worker alive?
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * If you need to keep the worker alive, you must use StrongWorkerRef.
 * You can have this refcounted, NON thread-safe object, calling
 * mozilla::dom::StrongWorkerRef::Create(WorkerPrivate* aWorkerPrivate);
 *
 * If you have a StrongWorkerRef:
 * a. the DOM Worker is kept alive.
 * b. you can have access to the WorkerPrivate, calling: Private().
 * c. WorkerControlRunnable can be dispatched.
 *
 * Note that the DOM Worker shutdown can start at any time, but having a
 * StrongWorkerRef prevents the full shutdown. Also with StrongWorkerRef, you
 * can pass a callback when calling mozilla::dom::StrongWorkerRef::Create().
 *
 * When the DOM Worker shutdown starts, WorkerRunnable cannot be dispatched
 * anymore. At this point, you should dispatch WorkerControlRunnable just to
 * release resources.
 *
 * How to have a thread-safe DOM Worker reference?
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Sometimes you need to play with threads and you need a thread-safe worker
 * reference. ThreadSafeWorkerRef is what you want.
 *
 * Just because this object can be sent to different threads, we don't allow the
 * setting of a callback. It would be confusing.
 *
 * ThreadSafeWorkerRef can be destroyed in any thread. Internally it keeps a
 * reference to its StrongWorkerRef creator and this ref will be dropped on the
 * correct thread when the ThreadSafeWorkerRef is deleted.
 *
 * IPC WorkerRef
 * ~~~~~~~~~~~~~
 *
 * IPDL protocols require a correct shutdown sequence. Because of this, they
 * need a special configuration:
 * 1. they need to be informed when the Worker starts the shutting down
 * 2. they don't want to prevent the shutdown
 * 3. but at the same time, they need to block the shutdown until the WorkerRef
 *    is not longer alive.
 *
 * Point 1 is a standard feature of WorkerRef; point 2 is similar to
 * WeakWorkerRef; point 3 is similar to StrongWorkerRef.
 *
 * You can create a special IPC WorkerRef using this static method:
 * mozilla::dom::IPCWorkerRef::Create(WorkerPrivate* aWorkerPrivate,
 *                                    const char* * aName);
 */

class WorkerPrivate;
class StrongWorkerRef;
class ThreadSafeWorkerRef;

class WorkerRef {
  friend class WorkerPrivate;

 public:
  NS_INLINE_DECL_REFCOUNTING(WorkerRef)

 protected:
  WorkerRef(WorkerPrivate* aWorkerPrivate, const char* aName,
            bool aIsPreventingShutdown);
  virtual ~WorkerRef();

  virtual void Notify();

  bool HoldWorker(WorkerStatus aStatus);
  void ReleaseWorker();

  bool IsPreventingShutdown() const { return mIsPreventingShutdown; }

  const char* Name() const { return mName; }

  WorkerPrivate* mWorkerPrivate;

  MoveOnlyFunction<void()> mCallback;
  const char* const mName;
  const bool mIsPreventingShutdown;

  // True if this WorkerRef has been added to a WorkerPrivate.
  bool mHolding;
};

class WeakWorkerRef final : public WorkerRef {
 public:
  static already_AddRefed<WeakWorkerRef> Create(
      WorkerPrivate* aWorkerPrivate,
      MoveOnlyFunction<void()>&& aCallback = nullptr);

  WorkerPrivate* GetPrivate() const;

  // This can be called on any thread. It's racy and, in general, the wrong
  // choice.
  WorkerPrivate* GetUnsafePrivate() const;

 private:
  explicit WeakWorkerRef(WorkerPrivate* aWorkerPrivate);
  ~WeakWorkerRef();

  void Notify() override;
};

class StrongWorkerRef final : public WorkerRef {
 public:
  static already_AddRefed<StrongWorkerRef> Create(
      WorkerPrivate* aWorkerPrivate, const char* aName,
      MoveOnlyFunction<void()>&& aCallback = nullptr);

  // This function creates a StrongWorkerRef even when in the Canceling state of
  // the worker's lifecycle. It's intended to be used by system code, e.g. code
  // that needs to perform IPC.
  //
  // This method should only be used in cases where the StrongWorkerRef will be
  // used for an extremely bounded duration that cannot be impacted by content.
  // For example, IPCStreams use this type of ref in order to immediately
  // migrate to an actor on another thread. Whether the IPCStream ever actually
  // is streamed does not matter; the ref will be dropped once the new actor is
  // created. For this reason, this method does not take a callback. It's
  // expected and required that callers will drop the reference when they are
  // done.
  static already_AddRefed<StrongWorkerRef> CreateForcibly(
      WorkerPrivate* aWorkerPrivate, const char* aName);

  WorkerPrivate* Private() const;

 private:
  friend class WeakWorkerRef;
  friend class ThreadSafeWorkerRef;

  static already_AddRefed<StrongWorkerRef> CreateImpl(
      WorkerPrivate* aWorkerPrivate, const char* aName,
      WorkerStatus aFailStatus);

  StrongWorkerRef(WorkerPrivate* aWorkerPrivate, const char* aName);
  ~StrongWorkerRef();
};

class ThreadSafeWorkerRef final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ThreadSafeWorkerRef)

  explicit ThreadSafeWorkerRef(StrongWorkerRef* aRef);

  WorkerPrivate* Private() const;

 private:
  friend class StrongWorkerRef;

  ~ThreadSafeWorkerRef();

  RefPtr<StrongWorkerRef> mRef;
};

class IPCWorkerRef final : public WorkerRef {
 public:
  static already_AddRefed<IPCWorkerRef> Create(
      WorkerPrivate* aWorkerPrivate, const char* aName,
      MoveOnlyFunction<void()>&& aCallback = nullptr);

  WorkerPrivate* Private() const;

  void SetActorCount(uint32_t aCount);

 private:
  IPCWorkerRef(WorkerPrivate* aWorkerPrivate, const char* aName);
  ~IPCWorkerRef();

  // The count of background actors which binding with this IPCWorkerRef.
  uint32_t mActorCount;
};

// Template class to keep an Actor pointer, as a raw pointer, in a ref-counted
// way when passed to lambdas.
template <class ActorPtr>
class IPCWorkerRefHelper final {
 public:
  NS_INLINE_DECL_REFCOUNTING(IPCWorkerRefHelper);

  explicit IPCWorkerRefHelper(ActorPtr* aActor) : mActor(aActor) {}

  ActorPtr* Actor() const { return mActor; }

 private:
  ~IPCWorkerRefHelper() = default;

  // Raw pointer
  ActorPtr* mActor;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_workers_WorkerRef_h */
