/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerRef_h
#define mozilla_dom_workers_WorkerRef_h

#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerHolder.h"
#include "mozilla/UniquePtr.h"
#include <functional>

namespace mozilla {
namespace dom {

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
 * WorkerHolder.h. The DOM Worker thread will basically stop scheduling
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
 */

class WorkerPrivate;
class StrongWorkerRef;
class ThreadSafeWorkerRef;

class WorkerRef
{
public:
  NS_INLINE_DECL_REFCOUNTING(WorkerRef)

protected:
  class Holder;
  friend class Holder;

  explicit WorkerRef(WorkerPrivate* aWorkerPrivate);
  virtual ~WorkerRef();

  virtual void
  Notify();

  WorkerPrivate* mWorkerPrivate;
  UniquePtr<WorkerHolder> mHolder;

  std::function<void()> mCallback;
};

class WeakWorkerRef final : public WorkerRef
{
public:
  static already_AddRefed<WeakWorkerRef>
  Create(WorkerPrivate* aWorkerPrivate,
         const std::function<void()>& aCallback = nullptr);

  WorkerPrivate*
  GetPrivate() const;

  // This can be called on any thread. It's racy and, in general, the wrong
  // choice.
  WorkerPrivate*
  GetUnsafePrivate() const;

private:
  explicit WeakWorkerRef(WorkerPrivate* aWorkerPrivate);
  ~WeakWorkerRef();

  void
  Notify() override;
};

class StrongWorkerRef final : public WorkerRef
{
public:
  static already_AddRefed<StrongWorkerRef>
  Create(WorkerPrivate* aWorkerPrivate,
         const char* aName,
         const std::function<void()>& aCallback = nullptr);

  WorkerPrivate*
  Private() const;

private:
  friend class WeakWorkerRef;
  friend class ThreadSafeWorkerRef;

  explicit StrongWorkerRef(WorkerPrivate* aWorkerPrivate);
  ~StrongWorkerRef();
};

class ThreadSafeWorkerRef final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ThreadSafeWorkerRef)

  explicit ThreadSafeWorkerRef(StrongWorkerRef* aRef);

  WorkerPrivate*
  Private() const;

private:
  friend class StrongWorkerRef;

  ~ThreadSafeWorkerRef();

  RefPtr<StrongWorkerRef> mRef;
};

} // dom namespace
} // mozilla namespace

#endif /* mozilla_dom_workers_WorkerRef_h */
