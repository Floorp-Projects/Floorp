/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseWindowProxy_h
#define mozilla_dom_PromiseWindowProxy_h

#include "nsCOMPtr.h"
#include "mozilla/WeakPtr.h"

class nsIWeakReference;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class Promise;

// When a data structure is waiting to resolve a promise from a
// window, but that data structure is not owned by that window, there
// exists a dilemma:
//
// 1) If the promise is never resolved, and the non-window data
// structure is keeping alive the promise, the promise's window will
// be leaked.
//
// 2) Something has to keep the promise alive from native code in case
// script has dropped all direct references to it, but the promise is
// later resolved.
//
// PromiseWindowProxy solves this dilemma for the non-window data
// structure. It solves (1) by only keeping a weak reference to the
// promise. It solves (2) by adding a strong reference to the promise
// on the window itself. This ensures that as long as both the
// PromiseWindowProxy and the window are still alive that the promise
// will remain alive. This strong reference is cycle collected, so it
// won't cause the window to leak.
class PromiseWindowProxy final
{
public:
  PromiseWindowProxy(nsPIDOMWindowInner* aWindow, mozilla::dom::Promise* aPromise);
  ~PromiseWindowProxy();

  RefPtr<Promise> Get() const;
  nsCOMPtr<nsPIDOMWindowInner> GetWindow() const;

private:
  nsCOMPtr<nsIWeakReference> mWindow;
  WeakPtr<Promise> mPromise;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseWindowProxy_h
