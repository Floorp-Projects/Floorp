/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Dispatcher_h
#define mozilla_dom_Dispatcher_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/TaskCategory.h"
#include "nsISupports.h"

class nsIEventTarget;
class nsIRunnable;

// This file defines basic functionality for dispatching runnables to various
// groups: either the SystemGroup or a DocGroup or TabGroup. Ideally all
// runnables destined for the main thread should be dispatched to a group
// instead so that we know what kind of web content they'll be
// touching. Runnables sent to the SystemGroup never touch web
// content. Runnables sent to a DocGroup can only touch documents belonging to
// that DocGroup. Runnables sent to a TabGroup can touch any document in any of
// the tabs belonging to the TabGroup.

namespace mozilla {
class AbstractThread;
namespace dom {
class TabGroup;

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

  // Must be called on the main thread. The AbstractThread can always be used
  // off the main thread.
  virtual AbstractThread* AbstractMainThreadFor(TaskCategory aCategory);
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

  // Must be called on the main thread. The AbstractThread can always be used
  // off the main thread.
  virtual AbstractThread* AbstractMainThreadFor(TaskCategory aCategory) = 0;

  // This method performs a safe cast. It returns null if |this| is not of the
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
