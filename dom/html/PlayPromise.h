/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PlayPromise_h__
#define __PlayPromise_h__

#include "mozilla/dom/Promise.h"
#include "mozilla/Telemetry.h"

namespace mozilla {
namespace dom {

// Decorates a DOM Promise to report telemetry as to whether it was resolved
// or rejected and why.
class PlayPromise : public Promise
{
public:
  static already_AddRefed<PlayPromise> Create(nsIGlobalObject* aGlobal,
                                              ErrorResult& aRv);
  ~PlayPromise();
  void MaybeResolveWithUndefined();
  void MaybeReject(nsresult aReason);

private:
  explicit PlayPromise(nsIGlobalObject* aGlobal);
  bool mFulfilled = false;
};

} // namespace dom
} // namespace mozilla

#endif // __PlayPromise_h__
