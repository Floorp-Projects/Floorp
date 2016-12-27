/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Dispatcher_h
#define mozilla_dom_Dispatcher_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsISupports.h"

class nsIEventTarget;
class nsIRunnable;

namespace mozilla {
namespace dom {

class TabGroup;
class DocGroup;

enum class TaskCategory {
  // User input (clicks, keypresses, etc.)
  UI,

  // Data from the network
  Network,

  // setTimeout, setInterval
  Timer,

  // Runnables posted from a worker to the main thread
  Worker,

  // requestIdleCallback
  IdleCallback,

  // Vsync notifications
  RefreshDriver,

  // Most DOM events (postMessage, media, plugins)
  Other,

  Count
};

// This trait should be attached to classes like nsIGlobalObject and nsIDocument
// that have a DocGroup or TabGroup attached to them. The methods here should
// delegate to the DocGroup or TabGroup. We can't use the Dispatcher class
// directly because it inherits from nsISupports.
class DispatcherTrait {
public:
  // This method may or may not be safe off of the main thread. For nsIDocument
  // it is safe. For nsIGlobalWindow it is not safe.
  virtual nsresult Dispatch(const char* aName,
                            TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable);

  // This method may or may not be safe off of the main thread. For nsIDocument
  // it is safe. For nsIGlobalWindow it is not safe. The nsIEventTarget can
  // always be used off the main thread.
  virtual nsIEventTarget* EventTargetFor(TaskCategory aCategory) const;
};

// Base class for DocGroup and TabGroup.
class Dispatcher : public nsISupports {
public:
  // This method is always safe to call off the main thread.
  virtual nsresult Dispatch(const char* aName,
                            TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) = 0;

  // This method is always safe to call off the main thread. The nsIEventTarget
  // can always be used off the main thread.
  virtual nsIEventTarget* EventTargetFor(TaskCategory aCategory) const = 0;

  // These methods perform a safe cast. They return null if |this| is not of the
  // requested type.
  virtual TabGroup* AsTabGroup() { return nullptr; }

protected:
  virtual already_AddRefed<nsIEventTarget>
  CreateEventTargetFor(TaskCategory aCategory);

  static Dispatcher* FromEventTarget(nsIEventTarget* aEventTarget);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Dispatcher_h
