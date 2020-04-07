/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DispatcherTrait_h
#define mozilla_dom_DispatcherTrait_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/TaskCategory.h"

class nsIRunnable;
class nsISerialEventTarget;

namespace mozilla {
class AbstractThread;
namespace dom {
// This trait should be attached to classes like nsIGlobalObject and
// Document that have a DocGroup attached to them. The methods here
// should delegate to the DocGroup. We can't use the
// Dispatcher class directly because it inherits from nsISupports.
class DispatcherTrait {
 public:
  // This method may or may not be safe off of the main thread. For Document it
  // is safe. For nsIGlobalWindow it is not safe.
  virtual nsresult Dispatch(TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable);

  // This method may or may not be safe off of the main thread. For Document it
  // is safe. For nsIGlobalWindow it is not safe. The nsISerialEventTarget can
  // always be used off the main thread.
  virtual nsISerialEventTarget* EventTargetFor(TaskCategory aCategory) const;

  // Must be called on the main thread. The AbstractThread can always be used
  // off the main thread.
  virtual AbstractThread* AbstractMainThreadFor(TaskCategory aCategory);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DispatcherTrait_h
