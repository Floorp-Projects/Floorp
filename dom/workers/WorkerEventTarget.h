/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkerEventTarget_h
#define mozilla_dom_WorkerEventTarget_h

#include "nsISerialEventTarget.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace dom {

class WorkerPrivate;

class WorkerEventTarget final : public nsISerialEventTarget {
 public:
  // The WorkerEventTarget supports different dispatch behaviors:
  //
  // * Hybrid targets will attempt to dispatch as a normal runnable,
  //   but fallback to a control runnable if that fails.  This is
  //   often necessary for code that wants normal dispatch order, but
  //   also needs to execute while the worker is shutting down (possibly
  //   with a holder in place.)
  //
  // * ControlOnly targets will simply dispatch a control runnable.
  enum class Behavior : uint8_t { Hybrid, ControlOnly };

 private:
  mozilla::Mutex mMutex;
  WorkerPrivate* mWorkerPrivate;
  const Behavior mBehavior;

  ~WorkerEventTarget() = default;

 public:
  WorkerEventTarget(WorkerPrivate* aWorkerPrivate, Behavior aBehavior);

  void ForgetWorkerPrivate(WorkerPrivate* aWorkerPrivate);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET
  NS_DECL_NSISERIALEVENTTARGET
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_WorkerEventTarget_h
