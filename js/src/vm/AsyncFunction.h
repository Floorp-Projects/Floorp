/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_AsyncFunction_h
#define vm_AsyncFunction_h

#include "js/Class.h"
#include "vm/AsyncFunctionResolveKind.h"  // AsyncFunctionResolveKind
#include "vm/GeneratorObject.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/PromiseObject.h"

namespace js {

class AsyncFunctionGeneratorObject;

extern const JSClass AsyncFunctionClass;

// Resume the async function when the `await` operand resolves.
// Split into two functions depending on whether the awaited value was
// fulfilled or rejected.
MOZ_MUST_USE bool AsyncFunctionAwaitedFulfilled(
    JSContext* cx, Handle<AsyncFunctionGeneratorObject*> generator,
    HandleValue value);

MOZ_MUST_USE bool AsyncFunctionAwaitedRejected(
    JSContext* cx, Handle<AsyncFunctionGeneratorObject*> generator,
    HandleValue reason);

// Resolve the async function's promise object with the given value and then
// return the promise object.
JSObject* AsyncFunctionResolve(JSContext* cx,
                               Handle<AsyncFunctionGeneratorObject*> generator,
                               HandleValue valueOrReason,
                               AsyncFunctionResolveKind resolveKind);

class AsyncFunctionGeneratorObject : public AbstractGeneratorObject {
 public:
  enum {
    PROMISE_SLOT = AbstractGeneratorObject::RESERVED_SLOTS,

    RESERVED_SLOTS
  };

  static const JSClass class_;
  static const JSClassOps classOps_;

  static AsyncFunctionGeneratorObject* create(JSContext* cx,
                                              HandleFunction asyncGen);

  PromiseObject* promise() {
    return &getFixedSlot(PROMISE_SLOT).toObject().as<PromiseObject>();
  }
};

}  // namespace js

#endif /* vm_AsyncFunction_h */
