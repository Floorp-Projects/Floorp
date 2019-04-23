/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/AsyncFunction.h"

#include "mozilla/Maybe.h"

#include "builtin/Promise.h"
#include "vm/GeneratorObject.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/Realm.h"
#include "vm/SelfHosting.h"

#include "vm/JSObject-inl.h"

using namespace js;

using mozilla::Maybe;

/* static */
bool GlobalObject::initAsyncFunction(JSContext* cx,
                                     Handle<GlobalObject*> global) {
  if (global->getReservedSlot(ASYNC_FUNCTION_PROTO).isObject()) {
    return true;
  }

  RootedObject asyncFunctionProto(
      cx, NewSingletonObjectWithFunctionPrototype(cx, global));
  if (!asyncFunctionProto) {
    return false;
  }

  if (!DefineToStringTag(cx, asyncFunctionProto, cx->names().AsyncFunction)) {
    return false;
  }

  RootedObject proto(
      cx, GlobalObject::getOrCreateFunctionConstructor(cx, cx->global()));
  if (!proto) {
    return false;
  }
  HandlePropertyName name = cx->names().AsyncFunction;
  RootedObject asyncFunction(
      cx, NewFunctionWithProto(cx, AsyncFunctionConstructor, 1,
                               JSFunction::NATIVE_CTOR, nullptr, name, proto));
  if (!asyncFunction) {
    return false;
  }
  if (!LinkConstructorAndPrototype(cx, asyncFunction, asyncFunctionProto,
                                   JSPROP_PERMANENT | JSPROP_READONLY,
                                   JSPROP_READONLY)) {
    return false;
  }

  global->setReservedSlot(ASYNC_FUNCTION, ObjectValue(*asyncFunction));
  global->setReservedSlot(ASYNC_FUNCTION_PROTO,
                          ObjectValue(*asyncFunctionProto));
  return true;
}

enum class ResumeKind { Normal, Throw };

// ES2020 draft rev a09fc232c137800dbf51b6204f37fdede4ba1646
// 6.2.3.1.1 Await Fulfilled Functions
// 6.2.3.1.2 Await Rejected Functions
static bool AsyncFunctionResume(JSContext* cx,
                                Handle<AsyncFunctionGeneratorObject*> generator,
                                ResumeKind kind, HandleValue valueOrReason) {
  // We're enqueuing the promise job for Await before suspending the execution
  // of the async function. So when either the debugger or OOM errors terminate
  // the execution after JSOP_ASYNCAWAIT, but before JSOP_AWAIT, we're in an
  // inconsistent state, because we don't have a resume index set and therefore
  // don't know where to resume the async function. Return here in that case.
  if (generator->isClosed()) {
    return true;
  }

  // The debugger sets the async function's generator object into the "running"
  // state while firing debugger events to ensure the debugger can't re-enter
  // the async function, cf. |AutoSetGeneratorRunning| in Debugger.cpp. Catch
  // this case here by checking if the generator is already runnning.
  if (generator->isRunning()) {
    return true;
  }

  Rooted<PromiseObject*> resultPromise(cx, generator->promise());

  RootedObject stack(cx);
  Maybe<JS::AutoSetAsyncStackForNewCalls> asyncStack;
  if (JSObject* allocationSite = resultPromise->allocationSite()) {
    // The promise is created within the activation of the async function, so
    // use the parent frame as the starting point for async stacks.
    stack = allocationSite->as<SavedFrame>().getParent();
    if (stack) {
      asyncStack.emplace(
          cx, stack, "async",
          JS::AutoSetAsyncStackForNewCalls::AsyncCallKind::EXPLICIT);
    }
  }

  MOZ_ASSERT(generator->isSuspended(),
             "non-suspended generator when resuming async function");

  // Execution context switching is handled in generator.
  HandlePropertyName funName = kind == ResumeKind::Normal
                                   ? cx->names().AsyncFunctionNext
                                   : cx->names().AsyncFunctionThrow;
  FixedInvokeArgs<1> args(cx);
  args[0].set(valueOrReason);
  RootedValue generatorOrValue(cx, ObjectValue(*generator));
  if (!CallSelfHostedFunction(cx, funName, generatorOrValue, args,
                              &generatorOrValue)) {
    if (!generator->isClosed()) {
      generator->setClosed();
    }

    // Handle the OOM case mentioned above.
    if (resultPromise->state() == JS::PromiseState::Pending &&
        cx->isExceptionPending()) {
      RootedValue exn(cx);
      if (!GetAndClearException(cx, &exn)) {
        return false;
      }
      return AsyncFunctionThrown(cx, resultPromise, exn);
    }
    return false;
  }

  MOZ_ASSERT_IF(generator->isClosed(), generatorOrValue.isObject());
  MOZ_ASSERT_IF(generator->isClosed(),
                &generatorOrValue.toObject() == resultPromise);
  MOZ_ASSERT_IF(!generator->isClosed(), generator->isAfterAwait());

  return true;
}

// ES2020 draft rev a09fc232c137800dbf51b6204f37fdede4ba1646
// 6.2.3.1.1 Await Fulfilled Functions
MOZ_MUST_USE bool js::AsyncFunctionAwaitedFulfilled(
    JSContext* cx, Handle<AsyncFunctionGeneratorObject*> generator,
    HandleValue value) {
  return AsyncFunctionResume(cx, generator, ResumeKind::Normal, value);
}

// ES2020 draft rev a09fc232c137800dbf51b6204f37fdede4ba1646
// 6.2.3.1.2 Await Rejected Functions
MOZ_MUST_USE bool js::AsyncFunctionAwaitedRejected(
    JSContext* cx, Handle<AsyncFunctionGeneratorObject*> generator,
    HandleValue reason) {
  return AsyncFunctionResume(cx, generator, ResumeKind::Throw, reason);
}

JSObject* js::AsyncFunctionResolve(
    JSContext* cx, Handle<AsyncFunctionGeneratorObject*> generator,
    HandleValue valueOrReason, AsyncFunctionResolveKind resolveKind) {
  Rooted<PromiseObject*> promise(cx, generator->promise());
  if (resolveKind == AsyncFunctionResolveKind::Fulfill) {
    if (!AsyncFunctionReturned(cx, promise, valueOrReason)) {
      return nullptr;
    }
  } else {
    if (!AsyncFunctionThrown(cx, promise, valueOrReason)) {
      return nullptr;
    }
  }
  return promise;
}

const Class AsyncFunctionGeneratorObject::class_ = {
    "AsyncFunctionGenerator",
    JSCLASS_HAS_RESERVED_SLOTS(AsyncFunctionGeneratorObject::RESERVED_SLOTS)};

AsyncFunctionGeneratorObject* AsyncFunctionGeneratorObject::create(
    JSContext* cx, HandleFunction fun) {
  MOZ_ASSERT(fun->isAsync() && !fun->isGenerator());

  Rooted<PromiseObject*> resultPromise(cx, CreatePromiseObjectForAsync(cx));
  if (!resultPromise) {
    return nullptr;
  }

  auto* obj = NewBuiltinClassInstance<AsyncFunctionGeneratorObject>(cx);
  if (!obj) {
    return nullptr;
  }
  obj->initFixedSlot(PROMISE_SLOT, ObjectValue(*resultPromise));

  // Starts in the running state.
  obj->setResumeIndex(AbstractGeneratorObject::RESUME_INDEX_RUNNING);

  return obj;
}
