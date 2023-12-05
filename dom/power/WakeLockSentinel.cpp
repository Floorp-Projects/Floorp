/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WakeLockSentinelBinding.h"
#include "WakeLockSentinel.h"

namespace mozilla::dom {

JSObject* WakeLockSentinel::WrapObject(JSContext* cx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return WakeLockSentinel_Binding::Wrap(cx, this, aGivenProto);
}

bool WakeLockSentinel::Released() const { return false; }

already_AddRefed<Promise> WakeLockSentinel::ReleaseLock() { return nullptr; }

}  // namespace mozilla::dom
