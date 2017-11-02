/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerHolderToken_h
#define mozilla_dom_workers_WorkerHolderToken_h

#include "nsISupportsImpl.h"
#include "nsTObserverArray.h"
#include "WorkerHolder.h"

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

// This is a ref-counted WorkerHolder implementation.  If you wish
// to be notified of worker shutdown beginning, then you can implement
// the Listener interface and call AddListener().
//
// This is purely a convenience class to avoid requiring code to
// extend WorkerHolder all the time.
class WorkerHolderToken final : public WorkerHolder
{
public:
  // Pure virtual class defining the interface for objects that
  // wish to be notified of worker shutdown.
  class Listener
  {
  public:
    virtual void
    WorkerShuttingDown() = 0;
  };

  // Attempt to create a WorkerHolderToken().  If the shutdown has already
  // passed the given shutdown phase or fails for another reason then
  // nullptr is returned.
  static already_AddRefed<WorkerHolderToken>
  Create(workers::WorkerPrivate* aWorkerPrivate, Status aShutdownStatus,
         Behavior aBehavior = PreventIdleShutdownStart);

  // Add a listener to the token.  Note, this does not hold a strong
  // reference to the listener.  You must call RemoveListener() before
  // the listener is destroyed.  This can only be called on the owning
  // worker thread.
  void
  AddListener(Listener* aListener);

  // Remove a previously added listener.  This can only be called on the
  // owning worker thread.
  void
  RemoveListener(Listener* aListener);

  bool
  IsShuttingDown() const;

  WorkerPrivate*
  GetWorkerPrivate() const;

private:
  WorkerHolderToken(Status aShutdownStatus, Behavior aBehavior);

  ~WorkerHolderToken();

  // WorkerHolder methods
  virtual bool
  Notify(workers::Status aStatus) override;

  nsTObserverArray<Listener*> mListenerList;
  const Status mShutdownStatus;
  bool mShuttingDown;

public:
  NS_INLINE_DECL_REFCOUNTING(WorkerHolderToken)
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_WorkerHolderToken_h
