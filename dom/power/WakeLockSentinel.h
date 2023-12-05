/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WAKELOCKSENTINEL_H_
#define DOM_WAKELOCKSENTINEL_H_

#include "js/TypeDecls.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WakeLockBinding.h"

namespace mozilla::dom {

class Promise;

}  // namespace mozilla::dom

namespace mozilla::dom {

class WakeLockSentinel final : public DOMEventTargetHelper {
 public:
  WakeLockSentinel(nsIGlobalObject* aOwnerWindow, WakeLockType aType)
      : DOMEventTargetHelper(aOwnerWindow), mType(aType) {}

 protected:
  ~WakeLockSentinel() = default;

 public:
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  bool Released() const;

  WakeLockType Type() const { return mType; }

  MOZ_CAN_RUN_SCRIPT
  already_AddRefed<Promise> ReleaseLock();

  IMPL_EVENT_HANDLER(release);

 private:
  WakeLockType mType;
};

}  // namespace mozilla::dom

#endif  // DOM_WAKELOCKSENTINEL_H_
