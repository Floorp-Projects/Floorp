/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS Atomics pseudo-module.
 *
 * See "Spec: JavaScript Shared Memory, Atomics, and Locks" for the
 * full specification.
 *
 * In addition to what is specified there, we throw an Error object if
 * the futex API hooks have not been installed on the runtime.
 * Essentially that is an implementation error at a higher level.
 *
 *
 * Note on the current implementation of atomic operations.
 *
 * The Mozilla atomics are not sufficient to implement these APIs
 * because we need to support 8-bit, 16-bit, and 32-bit data: the
 * Mozilla atomics only support 32-bit data.
 *
 * At the moment we include mozilla/Atomics.h, which will define
 * MOZ_HAVE_CXX11_ATOMICS and include <atomic> if we have C++11
 * atomics.
 *
 * If MOZ_HAVE_CXX11_ATOMICS is set we'll use C++11 atomics.
 *
 * Otherwise, if the compiler has them we'll fall back on gcc/Clang
 * intrinsics.
 *
 * Otherwise, if we're on VC++2012, we'll use C++11 atomics even if
 * MOZ_HAVE_CXX11_ATOMICS is not defined.  The compiler has the
 * atomics but they are disabled in Mozilla due to a performance bug.
 * That performance bug does not affect the Atomics code.  See
 * mozilla/Atomics.h for further comments on that bug.
 *
 * Otherwise, if we're on VC++2010 or VC++2008, we'll emulate the
 * gcc/Clang intrinsics with simple code below using the VC++
 * intrinsics, like the VC++2012 solution this is a stopgap since
 * we're about to start using VC++2013 anyway.
 *
 * If none of those options are available then the build must disable
 * shared memory, or compilation will fail with a predictable error.
 */

#include "builtin/AtomicsObject.h"

#include "mozilla/Atomics.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsnum.h"

#include "jit/AtomicOperations.h"
#include "jit/InlinableNatives.h"
#include "js/Class.h"
#include "js/PropertySpec.h"
#include "js/Result.h"
#include "vm/GlobalObject.h"
#include "vm/Time.h"
#include "vm/TypedArrayObject.h"
#include "wasm/WasmInstance.h"

#include "vm/JSObject-inl.h"

using namespace js;

static bool ReportBadArrayType(JSContext* cx) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_ATOMICS_BAD_ARRAY);
  return false;
}

static bool ReportOutOfRange(JSContext* cx) {
  // Use JSMSG_BAD_INDEX here, it is what ToIndex uses for some cases that it
  // reports directly.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_INDEX);
  return false;
}

static bool GetSharedTypedArray(JSContext* cx, HandleValue v, bool waitable,
                                MutableHandle<TypedArrayObject*> viewp) {
  if (!v.isObject()) {
    return ReportBadArrayType(cx);
  }
  if (!v.toObject().is<TypedArrayObject>()) {
    return ReportBadArrayType(cx);
  }
  viewp.set(&v.toObject().as<TypedArrayObject>());
  if (!viewp->isSharedMemory()) {
    return ReportBadArrayType(cx);
  }
  if (waitable) {
    switch (viewp->type()) {
      case Scalar::Int32:
      case Scalar::BigInt64:
        break;
      default:
        return ReportBadArrayType(cx);
    }
  } else {
    switch (viewp->type()) {
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Int16:
      case Scalar::Uint16:
      case Scalar::Int32:
      case Scalar::Uint32:
      case Scalar::BigInt64:
      case Scalar::BigUint64:
        break;
      default:
        return ReportBadArrayType(cx);
    }
  }
  return true;
}

static bool GetTypedArrayIndex(JSContext* cx, HandleValue v,
                               Handle<TypedArrayObject*> view,
                               uint32_t* offset) {
  uint64_t index;
  if (!ToIndex(cx, v, &index)) {
    return false;
  }
  if (index >= view->length()) {
    return ReportOutOfRange(cx);
  }
  *offset = uint32_t(index);
  return true;
}

template <typename T>
struct ArrayOps {
  static JS::Result<T> convertValue(JSContext* cx, HandleValue v) {
    int32_t n;
    if (!ToInt32(cx, v, &n)) {
      return cx->alreadyReportedError();
    }
    return (T)n;
  }

  static JS::Result<T> convertValue(JSContext* cx, HandleValue v,
                                    MutableHandleValue result) {
    double d;
    if (!ToInteger(cx, v, &d)) {
      return cx->alreadyReportedError();
    }
    result.setNumber(d);
    return (T)JS::ToInt32(d);
  }

  static JS::Result<> storeResult(JSContext* cx, T v,
                                  MutableHandleValue result) {
    result.setInt32(v);
    return Ok();
  }
};

template <>
JS::Result<> ArrayOps<uint32_t>::storeResult(JSContext* cx, uint32_t v,
                                             MutableHandleValue result) {
  result.setNumber(v);
  return Ok();
}

template <>
struct ArrayOps<int64_t> {
  static JS::Result<int64_t> convertValue(JSContext* cx, HandleValue v) {
    BigInt* bi = ToBigInt(cx, v);
    if (!bi) {
      return cx->alreadyReportedError();
    }
    return BigInt::toInt64(bi);
  }

  static JS::Result<int64_t> convertValue(JSContext* cx, HandleValue v,
                                          MutableHandleValue result) {
    BigInt* bi = ToBigInt(cx, v);
    if (!bi) {
      return cx->alreadyReportedError();
    }
    result.setBigInt(bi);
    return BigInt::toInt64(bi);
  }

  static JS::Result<> storeResult(JSContext* cx, int64_t v,
                                  MutableHandleValue result) {
    BigInt* bi = BigInt::createFromInt64(cx, v);
    if (!bi) {
      return cx->alreadyReportedError();
    }
    result.setBigInt(bi);
    return Ok();
  }
};

template <>
struct ArrayOps<uint64_t> {
  static JS::Result<uint64_t> convertValue(JSContext* cx, HandleValue v) {
    BigInt* bi = ToBigInt(cx, v);
    if (!bi) {
      return cx->alreadyReportedError();
    }
    return BigInt::toUint64(bi);
  }

  static JS::Result<uint64_t> convertValue(JSContext* cx, HandleValue v,
                                           MutableHandleValue result) {
    BigInt* bi = ToBigInt(cx, v);
    if (!bi) {
      return cx->alreadyReportedError();
    }
    result.setBigInt(bi);
    return BigInt::toUint64(bi);
  }

  static JS::Result<> storeResult(JSContext* cx, uint64_t v,
                                  MutableHandleValue result) {
    BigInt* bi = BigInt::createFromUint64(cx, v);
    if (!bi) {
      return cx->alreadyReportedError();
    }
    result.setBigInt(bi);
    return Ok();
  }
};

template <template <typename> class F, typename... Args>
bool perform(JSContext* cx, HandleValue objv, HandleValue idxv, Args... args) {
  Rooted<TypedArrayObject*> view(cx, nullptr);
  if (!GetSharedTypedArray(cx, objv, false, &view)) {
    return false;
  }
  uint32_t offset;
  if (!GetTypedArrayIndex(cx, idxv, view, &offset)) {
    return false;
  }
  SharedMem<void*> viewData = view->dataPointerShared();
  switch (view->type()) {
    case Scalar::Int8:
      return F<int8_t>::run(cx, viewData.cast<int8_t*>() + offset, args...);
    case Scalar::Uint8:
      return F<uint8_t>::run(cx, viewData.cast<uint8_t*>() + offset, args...);
    case Scalar::Int16:
      return F<int16_t>::run(cx, viewData.cast<int16_t*>() + offset, args...);
    case Scalar::Uint16:
      return F<uint16_t>::run(cx, viewData.cast<uint16_t*>() + offset, args...);
    case Scalar::Int32:
      return F<int32_t>::run(cx, viewData.cast<int32_t*>() + offset, args...);
    case Scalar::Uint32:
      return F<uint32_t>::run(cx, viewData.cast<uint32_t*>() + offset, args...);
    case Scalar::BigInt64:
      return F<int64_t>::run(cx, viewData.cast<int64_t*>() + offset, args...);
    case Scalar::BigUint64:
      return F<uint64_t>::run(cx, viewData.cast<uint64_t*>() + offset, args...);
    case Scalar::Float32:
    case Scalar::Float64:
    case Scalar::Uint8Clamped:
    case Scalar::MaxTypedArrayViewType:
    case Scalar::Int64:
      break;
  }
  MOZ_CRASH("Unsupported TypedArray type");
}

template <typename T>
struct DoCompareExchange {
  static bool run(JSContext* cx, SharedMem<T*> addr, HandleValue oldv,
                  HandleValue newv, MutableHandleValue result) {
    using Ops = ArrayOps<T>;
    T oldval;
    JS_TRY_VAR_OR_RETURN_FALSE(cx, oldval, Ops::convertValue(cx, oldv));
    T newval;
    JS_TRY_VAR_OR_RETURN_FALSE(cx, newval, Ops::convertValue(cx, newv));

    oldval = jit::AtomicOperations::compareExchangeSeqCst(addr, oldval, newval);

    JS_TRY_OR_RETURN_FALSE(cx, Ops::storeResult(cx, oldval, result));
    return true;
  }
};

bool js::atomics_compareExchange(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return perform<DoCompareExchange>(cx, args.get(0), args.get(1), args.get(2),
                                    args.get(3), args.rval());
}

template <typename T>
struct DoLoad {
  static bool run(JSContext* cx, SharedMem<T*> addr,
                  MutableHandleValue result) {
    using Ops = ArrayOps<T>;
    T v = jit::AtomicOperations::loadSeqCst(addr);
    JS_TRY_OR_RETURN_FALSE(cx, Ops::storeResult(cx, v, result));
    return true;
  }
};

bool js::atomics_load(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return perform<DoLoad>(cx, args.get(0), args.get(1), args.rval());
}

template <typename T>
struct DoExchange {
  static bool run(JSContext* cx, SharedMem<T*> addr, HandleValue valv,
                  MutableHandleValue result) {
    using Ops = ArrayOps<T>;
    T value;
    JS_TRY_VAR_OR_RETURN_FALSE(cx, value, Ops::convertValue(cx, valv));
    value = jit::AtomicOperations::exchangeSeqCst(addr, value);
    JS_TRY_OR_RETURN_FALSE(cx, Ops::storeResult(cx, value, result));
    return true;
  }
};

template <typename T>
struct DoStore {
  static bool run(JSContext* cx, SharedMem<T*> addr, HandleValue valv,
                  MutableHandleValue result) {
    using Ops = ArrayOps<T>;
    T value;
    JS_TRY_VAR_OR_RETURN_FALSE(cx, value, Ops::convertValue(cx, valv, result));
    jit::AtomicOperations::storeSeqCst(addr, value);
    return true;
  }
};

bool js::atomics_store(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return perform<DoStore>(cx, args.get(0), args.get(1), args.get(2),
                          args.rval());
}

bool js::atomics_exchange(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return perform<DoExchange>(cx, args.get(0), args.get(1), args.get(2),
                             args.rval());
}

template <typename Operate>
struct DoBinopWithOperation {
  template <typename T>
  struct DoBinop {
    static bool run(JSContext* cx, SharedMem<T*> addr, HandleValue valv,
                    MutableHandleValue result) {
      using Ops = ArrayOps<T>;
      T v;
      JS_TRY_VAR_OR_RETURN_FALSE(cx, v, Ops::convertValue(cx, valv));
      v = Operate::operate(addr, v);
      JS_TRY_OR_RETURN_FALSE(cx, Ops::storeResult(cx, v, result));
      return true;
    }
  };
};

template <typename Operate>
static bool AtomicsBinop(JSContext* cx, HandleValue objv, HandleValue idxv,
                         HandleValue valv, MutableHandleValue r) {
  return perform<DoBinopWithOperation<Operate>::template DoBinop>(
      cx, objv, idxv, valv, r);
}

#define INTEGRAL_TYPES_FOR_EACH(NAME)                              \
  static int8_t operate(SharedMem<int8_t*> addr, int8_t v) {       \
    return NAME(addr, v);                                          \
  }                                                                \
  static uint8_t operate(SharedMem<uint8_t*> addr, uint8_t v) {    \
    return NAME(addr, v);                                          \
  }                                                                \
  static int16_t operate(SharedMem<int16_t*> addr, int16_t v) {    \
    return NAME(addr, v);                                          \
  }                                                                \
  static uint16_t operate(SharedMem<uint16_t*> addr, uint16_t v) { \
    return NAME(addr, v);                                          \
  }                                                                \
  static int32_t operate(SharedMem<int32_t*> addr, int32_t v) {    \
    return NAME(addr, v);                                          \
  }                                                                \
  static uint32_t operate(SharedMem<uint32_t*> addr, uint32_t v) { \
    return NAME(addr, v);                                          \
  }                                                                \
  static int64_t operate(SharedMem<int64_t*> addr, int64_t v) {    \
    return NAME(addr, v);                                          \
  }                                                                \
  static uint64_t operate(SharedMem<uint64_t*> addr, uint64_t v) { \
    return NAME(addr, v);                                          \
  }

class PerformAdd {
 public:
  INTEGRAL_TYPES_FOR_EACH(jit::AtomicOperations::fetchAddSeqCst)
};

bool js::atomics_add(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return AtomicsBinop<PerformAdd>(cx, args.get(0), args.get(1), args.get(2),
                                  args.rval());
}

class PerformSub {
 public:
  INTEGRAL_TYPES_FOR_EACH(jit::AtomicOperations::fetchSubSeqCst)
};

bool js::atomics_sub(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return AtomicsBinop<PerformSub>(cx, args.get(0), args.get(1), args.get(2),
                                  args.rval());
}

class PerformAnd {
 public:
  INTEGRAL_TYPES_FOR_EACH(jit::AtomicOperations::fetchAndSeqCst)
};

bool js::atomics_and(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return AtomicsBinop<PerformAnd>(cx, args.get(0), args.get(1), args.get(2),
                                  args.rval());
}

class PerformOr {
 public:
  INTEGRAL_TYPES_FOR_EACH(jit::AtomicOperations::fetchOrSeqCst)
};

bool js::atomics_or(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return AtomicsBinop<PerformOr>(cx, args.get(0), args.get(1), args.get(2),
                                 args.rval());
}

class PerformXor {
 public:
  INTEGRAL_TYPES_FOR_EACH(jit::AtomicOperations::fetchXorSeqCst)
};

bool js::atomics_xor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  return AtomicsBinop<PerformXor>(cx, args.get(0), args.get(1), args.get(2),
                                  args.rval());
}

bool js::atomics_isLockFree(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  HandleValue v = args.get(0);
  int32_t size;
  if (v.isInt32()) {
    size = v.toInt32();
  } else {
    double dsize;
    if (!ToInteger(cx, v, &dsize)) {
      return false;
    }
    if (!mozilla::NumberEqualsInt32(dsize, &size)) {
      args.rval().setBoolean(false);
      return true;
    }
  }
  args.rval().setBoolean(jit::AtomicOperations::isLockfreeJS(size));
  return true;
}

namespace js {

// Represents one waiting worker.
//
// The type is declared opaque in SharedArrayObject.h.  Instances of
// js::FutexWaiter are stack-allocated and linked onto a list across a
// call to FutexThread::wait().
//
// The 'waiters' field of the SharedArrayRawBuffer points to the highest
// priority waiter in the list, and lower priority nodes are linked through
// the 'lower_pri' field.  The 'back' field goes the other direction.
// The list is circular, so the 'lower_pri' field of the lowest priority
// node points to the first node in the list.  The list has no dedicated
// header node.

class FutexWaiter {
 public:
  FutexWaiter(uint32_t offset, JSContext* cx)
      : offset(offset), cx(cx), lower_pri(nullptr), back(nullptr) {}

  uint32_t offset;         // int32 element index within the SharedArrayBuffer
  JSContext* cx;           // The waiting thread
  FutexWaiter* lower_pri;  // Lower priority nodes in circular doubly-linked
                           // list of waiters
  FutexWaiter* back;       // Other direction
};

class AutoLockFutexAPI {
  // We have to wrap this in a Maybe because of the way loading
  // mozilla::Atomic pointers works.
  mozilla::Maybe<js::UniqueLock<js::Mutex>> unique_;

 public:
  AutoLockFutexAPI() {
    js::Mutex* lock = FutexThread::lock_;
    unique_.emplace(*lock);
  }

  ~AutoLockFutexAPI() { unique_.reset(); }

  js::UniqueLock<js::Mutex>& unique() { return *unique_; }
};

}  // namespace js

template <typename T>
static FutexThread::WaitResult AtomicsWait(
    JSContext* cx, SharedArrayRawBuffer* sarb, uint32_t byteOffset, T value,
    const mozilla::Maybe<mozilla::TimeDuration>& timeout) {
  // Validation and other guards should ensure that this does not happen.
  MOZ_ASSERT(sarb, "wait is only applicable to shared memory");

  if (!cx->fx.canWait()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_ATOMICS_WAIT_NOT_ALLOWED);
    return FutexThread::WaitResult::Error;
  }

  SharedMem<T*> addr =
      sarb->dataPointerShared().cast<T*>() + (byteOffset / sizeof(T));

  // This lock also protects the "waiters" field on SharedArrayRawBuffer,
  // and it provides the necessary memory fence.
  AutoLockFutexAPI lock;

  if (jit::AtomicOperations::loadSafeWhenRacy(addr) != value) {
    return FutexThread::WaitResult::NotEqual;
  }

  FutexWaiter w(byteOffset, cx);
  if (FutexWaiter* waiters = sarb->waiters()) {
    w.lower_pri = waiters;
    w.back = waiters->back;
    waiters->back->lower_pri = &w;
    waiters->back = &w;
  } else {
    w.lower_pri = w.back = &w;
    sarb->setWaiters(&w);
  }

  FutexThread::WaitResult retval = cx->fx.wait(cx, lock.unique(), timeout);

  if (w.lower_pri == &w) {
    sarb->setWaiters(nullptr);
  } else {
    w.lower_pri->back = w.back;
    w.back->lower_pri = w.lower_pri;
    if (sarb->waiters() == &w) {
      sarb->setWaiters(w.lower_pri);
    }
  }

  return retval;
}

FutexThread::WaitResult js::atomics_wait_impl(
    JSContext* cx, SharedArrayRawBuffer* sarb, uint32_t byteOffset,
    int32_t value, const mozilla::Maybe<mozilla::TimeDuration>& timeout) {
  return AtomicsWait(cx, sarb, byteOffset, value, timeout);
}

FutexThread::WaitResult js::atomics_wait_impl(
    JSContext* cx, SharedArrayRawBuffer* sarb, uint32_t byteOffset,
    int64_t value, const mozilla::Maybe<mozilla::TimeDuration>& timeout) {
  return AtomicsWait(cx, sarb, byteOffset, value, timeout);
}

template <typename T>
static bool DoAtomicsWait(JSContext* cx, Handle<TypedArrayObject*> view,
                          uint32_t offset, T value, HandleValue timeoutv,
                          MutableHandleValue r) {
  mozilla::Maybe<mozilla::TimeDuration> timeout;
  if (!timeoutv.isUndefined()) {
    double timeout_ms;
    if (!ToNumber(cx, timeoutv, &timeout_ms)) {
      return false;
    }
    if (!mozilla::IsNaN(timeout_ms)) {
      if (timeout_ms < 0) {
        timeout = mozilla::Some(mozilla::TimeDuration::FromSeconds(0.0));
      } else if (!mozilla::IsInfinite(timeout_ms)) {
        timeout =
            mozilla::Some(mozilla::TimeDuration::FromMilliseconds(timeout_ms));
      }
    }
  }

  Rooted<SharedArrayBufferObject*> sab(cx, view->bufferShared());
  // The computation will not overflow because range checks have been
  // performed.
  uint32_t byteOffset =
      offset * sizeof(T) +
      (view->dataPointerShared().cast<uint8_t*>().unwrap(/* arithmetic */) -
       sab->dataPointerShared().unwrap(/* arithmetic */));

  switch (atomics_wait_impl(cx, sab->rawBufferObject(), byteOffset, value,
                            timeout)) {
    case FutexThread::WaitResult::NotEqual:
      r.setString(cx->names().futexNotEqual);
      return true;
    case FutexThread::WaitResult::OK:
      r.setString(cx->names().futexOK);
      return true;
    case FutexThread::WaitResult::TimedOut:
      r.setString(cx->names().futexTimedOut);
      return true;
    case FutexThread::WaitResult::Error:
      return false;
    default:
      MOZ_CRASH("Should not happen");
  }
}

bool js::atomics_wait(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  HandleValue objv = args.get(0);
  HandleValue idxv = args.get(1);
  HandleValue valv = args.get(2);
  HandleValue timeoutv = args.get(3);
  MutableHandleValue r = args.rval();

  Rooted<TypedArrayObject*> view(cx, nullptr);
  if (!GetSharedTypedArray(cx, objv, true, &view)) {
    return false;
  }
  MOZ_ASSERT(view->type() == Scalar::Int32 || view->type() == Scalar::BigInt64);

  uint32_t offset;
  if (!GetTypedArrayIndex(cx, idxv, view, &offset)) {
    return false;
  }

  if (view->type() == Scalar::Int32) {
    int32_t value;
    if (!ToInt32(cx, valv, &value)) {
      return false;
    }
    return DoAtomicsWait(cx, view, offset, value, timeoutv, r);
  }

  MOZ_ASSERT(view->type() == Scalar::BigInt64);
  RootedBigInt valbi(cx, ToBigInt(cx, valv));
  if (!valbi) {
    return false;
  }
  return DoAtomicsWait(cx, view, offset, BigInt::toInt64(valbi), timeoutv, r);
}

int64_t js::atomics_notify_impl(SharedArrayRawBuffer* sarb, uint32_t byteOffset,
                                int64_t count) {
  // Validation should ensure this does not happen.
  MOZ_ASSERT(sarb, "notify is only applicable to shared memory");

  AutoLockFutexAPI lock;

  int64_t woken = 0;

  FutexWaiter* waiters = sarb->waiters();
  if (waiters && count) {
    FutexWaiter* iter = waiters;
    do {
      FutexWaiter* c = iter;
      iter = iter->lower_pri;
      if (c->offset != byteOffset || !c->cx->fx.isWaiting()) {
        continue;
      }
      c->cx->fx.notify(FutexThread::NotifyExplicit);
      // Overflow will be a problem only in two cases:
      // (1) 128-bit systems with substantially more than 2^64 bytes of
      //     memory per process, and a very lightweight
      //     Atomics.waitAsync().  Obviously a future problem.
      // (2) Bugs.
      MOZ_RELEASE_ASSERT(woken < INT64_MAX);
      ++woken;
      if (count > 0) {
        --count;
      }
    } while (count && iter != waiters);
  }

  return woken;
}

bool js::atomics_notify(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  HandleValue objv = args.get(0);
  HandleValue idxv = args.get(1);
  HandleValue countv = args.get(2);
  MutableHandleValue r = args.rval();

  Rooted<TypedArrayObject*> view(cx, nullptr);
  if (!GetSharedTypedArray(cx, objv, true, &view)) {
    return false;
  }
  MOZ_ASSERT(view->type() == Scalar::Int32 || view->type() == Scalar::BigInt64);
  uint32_t elementSize =
      view->type() == Scalar::Int32 ? sizeof(int32_t) : sizeof(int64_t);
  uint32_t offset;
  if (!GetTypedArrayIndex(cx, idxv, view, &offset)) {
    return false;
  }
  int64_t count;
  if (countv.isUndefined()) {
    count = -1;
  } else {
    double dcount;
    if (!ToInteger(cx, countv, &dcount)) {
      return false;
    }
    if (dcount < 0.0) {
      dcount = 0.0;
    }
    count = dcount < double(1ULL << 63) ? int64_t(dcount) : -1;
  }

  Rooted<SharedArrayBufferObject*> sab(cx, view->bufferShared());
  // The computation will not overflow because range checks have been
  // performed.
  uint32_t byteOffset =
      offset * elementSize +
      (view->dataPointerShared().cast<uint8_t*>().unwrap(/* arithmetic */) -
       sab->dataPointerShared().unwrap(/* arithmetic */));

  r.setNumber(
      double(atomics_notify_impl(sab->rawBufferObject(), byteOffset, count)));

  return true;
}

/* static */
bool js::FutexThread::initialize() {
  MOZ_ASSERT(!lock_);
  lock_ = js_new<js::Mutex>(mutexid::FutexThread);
  return lock_ != nullptr;
}

/* static */
void js::FutexThread::destroy() {
  if (lock_) {
    js::Mutex* lock = lock_;
    js_delete(lock);
    lock_ = nullptr;
  }
}

/* static */
void js::FutexThread::lock() {
  // Load the atomic pointer.
  js::Mutex* lock = lock_;

  lock->lock();
}

/* static */ mozilla::Atomic<js::Mutex*, mozilla::SequentiallyConsistent>
    FutexThread::lock_;

/* static */
void js::FutexThread::unlock() {
  // Load the atomic pointer.
  js::Mutex* lock = lock_;

  lock->unlock();
}

js::FutexThread::FutexThread()
    : cond_(nullptr), state_(Idle), canWait_(false) {}

bool js::FutexThread::initInstance() {
  MOZ_ASSERT(lock_);
  cond_ = js_new<js::ConditionVariable>();
  return cond_ != nullptr;
}

void js::FutexThread::destroyInstance() {
  if (cond_) {
    js_delete(cond_);
  }
}

bool js::FutexThread::isWaiting() {
  // When a worker is awoken for an interrupt it goes into state
  // WaitingNotifiedForInterrupt for a short time before it actually
  // wakes up and goes into WaitingInterrupted.  In those states the
  // worker is still waiting, and if an explicit notify arrives the
  // worker transitions to Woken.  See further comments in
  // FutexThread::wait().
  return state_ == Waiting || state_ == WaitingInterrupted ||
         state_ == WaitingNotifiedForInterrupt;
}

FutexThread::WaitResult js::FutexThread::wait(
    JSContext* cx, js::UniqueLock<js::Mutex>& locked,
    const mozilla::Maybe<mozilla::TimeDuration>& timeout) {
  MOZ_ASSERT(&cx->fx == this);
  MOZ_ASSERT(cx->fx.canWait());
  MOZ_ASSERT(state_ == Idle || state_ == WaitingInterrupted);

  // Disallow waiting when a runtime is processing an interrupt.
  // See explanation below.

  if (state_ == WaitingInterrupted) {
    UnlockGuard<Mutex> unlock(locked);
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_ATOMICS_WAIT_NOT_ALLOWED);
    return WaitResult::Error;
  }

  // Go back to Idle after returning.
  auto onFinish = mozilla::MakeScopeExit([&] { state_ = Idle; });

  const bool isTimed = timeout.isSome();

  auto finalEnd = timeout.map([](const mozilla::TimeDuration& timeout) {
    return mozilla::TimeStamp::Now() + timeout;
  });

  // 4000s is about the longest timeout slice that is guaranteed to
  // work cross-platform.
  auto maxSlice = mozilla::TimeDuration::FromSeconds(4000.0);

  for (;;) {
    // If we are doing a timed wait, calculate the end time for this wait
    // slice.
    auto sliceEnd = finalEnd.map([&](mozilla::TimeStamp& finalEnd) {
      auto sliceEnd = mozilla::TimeStamp::Now() + maxSlice;
      if (finalEnd < sliceEnd) {
        sliceEnd = finalEnd;
      }
      return sliceEnd;
    });

    state_ = Waiting;

    MOZ_ASSERT((cx->runtime()->beforeWaitCallback == nullptr) ==
               (cx->runtime()->afterWaitCallback == nullptr));
    mozilla::DebugOnly<bool> callbacksPresent =
        cx->runtime()->beforeWaitCallback != nullptr;

    void* cookie = nullptr;
    uint8_t clientMemory[JS::WAIT_CALLBACK_CLIENT_MAXMEM];
    if (cx->runtime()->beforeWaitCallback) {
      cookie = (*cx->runtime()->beforeWaitCallback)(clientMemory);
    }

    if (isTimed) {
      mozilla::Unused << cond_->wait_until(locked, *sliceEnd);
    } else {
      cond_->wait(locked);
    }

    MOZ_ASSERT((cx->runtime()->afterWaitCallback != nullptr) ==
               callbacksPresent);
    if (cx->runtime()->afterWaitCallback) {
      (*cx->runtime()->afterWaitCallback)(cookie);
    }

    switch (state_) {
      case FutexThread::Waiting:
        // Timeout or spurious wakeup.
        if (isTimed) {
          auto now = mozilla::TimeStamp::Now();
          if (now >= *finalEnd) {
            return WaitResult::TimedOut;
          }
        }
        break;

      case FutexThread::Woken:
        return WaitResult::OK;

      case FutexThread::WaitingNotifiedForInterrupt:
        // The interrupt handler may reenter the engine.  In that case
        // there are two complications:
        //
        // - The waiting thread is not actually waiting on the
        //   condition variable so we have to record that it
        //   should be woken when the interrupt handler returns.
        //   To that end, we flag the thread as interrupted around
        //   the interrupt and check state_ when the interrupt
        //   handler returns.  A notify() call that reaches the
        //   runtime during the interrupt sets state_ to Woken.
        //
        // - It is in principle possible for wait() to be
        //   reentered on the same thread/runtime and waiting on the
        //   same location and to yet again be interrupted and enter
        //   the interrupt handler.  In this case, it is important
        //   that when another agent notifies waiters, all waiters using
        //   the same runtime on the same location are woken in LIFO
        //   order; FIFO may be the required order, but FIFO would
        //   fail to wake up the innermost call.  Interrupts are
        //   outside any spec anyway.  Also, several such suspended
        //   waiters may be woken at a time.
        //
        //   For the time being we disallow waiting from within code
        //   that runs from within an interrupt handler; this may
        //   occasionally (very rarely) be surprising but is
        //   expedient.  Other solutions exist, see bug #1131943.  The
        //   code that performs the check is above, at the head of
        //   this function.

        state_ = WaitingInterrupted;
        {
          UnlockGuard<Mutex> unlock(locked);
          if (!cx->handleInterrupt()) {
            return WaitResult::Error;
          }
        }
        if (state_ == Woken) {
          return WaitResult::OK;
        }
        break;

      default:
        MOZ_CRASH("Bad FutexState in wait()");
    }
  }
}

void js::FutexThread::notify(NotifyReason reason) {
  MOZ_ASSERT(isWaiting());

  if ((state_ == WaitingInterrupted || state_ == WaitingNotifiedForInterrupt) &&
      reason == NotifyExplicit) {
    state_ = Woken;
    return;
  }
  switch (reason) {
    case NotifyExplicit:
      state_ = Woken;
      break;
    case NotifyForJSInterrupt:
      if (state_ == WaitingNotifiedForInterrupt) {
        return;
      }
      state_ = WaitingNotifiedForInterrupt;
      break;
    default:
      MOZ_CRASH("bad NotifyReason in FutexThread::notify()");
  }
  cond_->notify_all();
}

const JSFunctionSpec AtomicsMethods[] = {
    JS_INLINABLE_FN("compareExchange", atomics_compareExchange, 4, 0,
                    AtomicsCompareExchange),
    JS_INLINABLE_FN("load", atomics_load, 2, 0, AtomicsLoad),
    JS_INLINABLE_FN("store", atomics_store, 3, 0, AtomicsStore),
    JS_INLINABLE_FN("exchange", atomics_exchange, 3, 0, AtomicsExchange),
    JS_INLINABLE_FN("add", atomics_add, 3, 0, AtomicsAdd),
    JS_INLINABLE_FN("sub", atomics_sub, 3, 0, AtomicsSub),
    JS_INLINABLE_FN("and", atomics_and, 3, 0, AtomicsAnd),
    JS_INLINABLE_FN("or", atomics_or, 3, 0, AtomicsOr),
    JS_INLINABLE_FN("xor", atomics_xor, 3, 0, AtomicsXor),
    JS_INLINABLE_FN("isLockFree", atomics_isLockFree, 1, 0, AtomicsIsLockFree),
    JS_FN("wait", atomics_wait, 4, 0),
    JS_FN("notify", atomics_notify, 3, 0),
    JS_FN("wake", atomics_notify, 3, 0),  // Legacy name
    JS_FS_END};

static const JSPropertySpec AtomicsProperties[] = {
    JS_STRING_SYM_PS(toStringTag, "Atomics", JSPROP_READONLY), JS_PS_END};

static JSObject* CreateAtomicsObject(JSContext* cx, JSProtoKey key) {
  Handle<GlobalObject*> global = cx->global();
  RootedObject proto(cx, GlobalObject::getOrCreateObjectPrototype(cx, global));
  if (!proto) {
    return nullptr;
  }
  return NewSingletonObjectWithGivenProto(cx, &AtomicsObject::class_, proto);
}

static const ClassSpec AtomicsClassSpec = {CreateAtomicsObject, nullptr,
                                           AtomicsMethods, AtomicsProperties};

const JSClass AtomicsObject::class_ = {
    "Atomics", JSCLASS_HAS_CACHED_PROTO(JSProto_Atomics), JS_NULL_CLASS_OPS,
    &AtomicsClassSpec};

#undef CXX11_ATOMICS
#undef GNU_ATOMICS
