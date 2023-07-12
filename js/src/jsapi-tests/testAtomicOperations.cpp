/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"

#include "jit/AtomicOperations.h"
#include "jsapi-tests/tests.h"
#include "vm/ArrayBufferObject.h"
#include "vm/SharedMem.h"
#include "vm/Uint8Clamped.h"
#include "wasm/WasmFeatures.h"

using namespace js;

// Machinery to disguise pointer addresses to the C++ compiler -- quite possibly
// not thread-safe.

extern void setHiddenPointer(void* p);
extern void* getHiddenPointer();

void* hidePointerValue(void* p) {
  setHiddenPointer(p);
  return getHiddenPointer();
}

//////////////////////////////////////////////////////////////////////
//
// Lock-freedom predicates

BEGIN_REUSABLE_TEST(testAtomicLockFree8) {
  // isLockfree8() must not return true if there are no 8-byte atomics

  CHECK(!jit::AtomicOperations::isLockfree8() ||
        jit::AtomicOperations::hasAtomic8());

  // We must have lock-free 8-byte atomics on every platform where we support
  // wasm, but we don't care otherwise.

  CHECK(!wasm::HasSupport(cx) || jit::AtomicOperations::isLockfree8());
  return true;
}
END_TEST(testAtomicLockFree8)

// The JS spec requires specific behavior for all but 1 and 2.

BEGIN_REUSABLE_TEST(testAtomicLockFreeJS) {
  static_assert(jit::AtomicOperations::isLockfreeJS(1) ==
                true);  // false is allowed by spec but not in SpiderMonkey
  static_assert(jit::AtomicOperations::isLockfreeJS(2) == true);   // ditto
  static_assert(jit::AtomicOperations::isLockfreeJS(8) == true);   // ditto
  static_assert(jit::AtomicOperations::isLockfreeJS(3) == false);  // required
  static_assert(jit::AtomicOperations::isLockfreeJS(4) == true);   // required
  static_assert(jit::AtomicOperations::isLockfreeJS(5) == false);  // required
  static_assert(jit::AtomicOperations::isLockfreeJS(6) == false);  // required
  static_assert(jit::AtomicOperations::isLockfreeJS(7) == false);  // required
  return true;
}
END_TEST(testAtomicLockFreeJS)

//////////////////////////////////////////////////////////////////////
//
// Fence

// This only tests that fenceSeqCst is defined and that it doesn't crash if we
// call it, but it has no return value and its effect is not observable here.

BEGIN_REUSABLE_TEST(testAtomicFence) {
  jit::AtomicOperations::fenceSeqCst();
  return true;
}
END_TEST(testAtomicFence)

//////////////////////////////////////////////////////////////////////
//
// Memory access primitives

// These tests for the atomic load and store primitives ascertain that the
// primitives are defined and that they load and store the values they should,
// but not that the primitives are actually atomic wrt to the memory subsystem.

// Memory for testing atomics.  This must be aligned to the natural alignment of
// the type we're testing; for now, use 8-byte alignment for all.

MOZ_ALIGNED_DECL(8, static uint8_t atomicMem[8]);
MOZ_ALIGNED_DECL(8, static uint8_t atomicMem2[8]);

// T is the primitive type we're testing, and A and B are references to constant
// bindings holding values of that type.
//
// No bytes of A and B should be 0 or FF.  A+B and A-B must not overflow.

#define ATOMIC_TESTS(T, A, B)                                           \
  T* q = (T*)hidePointerValue((void*)atomicMem);                        \
  *q = A;                                                               \
  SharedMem<T*> p =                                                     \
      SharedMem<T*>::shared((T*)hidePointerValue((T*)atomicMem));       \
  CHECK(*q == A);                                                       \
  CHECK(jit::AtomicOperations::loadSeqCst(p) == A);                     \
  CHECK(*q == A);                                                       \
  jit::AtomicOperations::storeSeqCst(p, B);                             \
  CHECK(*q == B);                                                       \
  CHECK(jit::AtomicOperations::exchangeSeqCst(p, A) == B);              \
  CHECK(*q == A);                                                       \
  CHECK(jit::AtomicOperations::compareExchangeSeqCst(p, (T)0, (T)1) ==  \
        A); /*failure*/                                                 \
  CHECK(*q == A);                                                       \
  CHECK(jit::AtomicOperations::compareExchangeSeqCst(p, A, B) ==        \
        A); /*success*/                                                 \
  CHECK(*q == B);                                                       \
  *q = A;                                                               \
  CHECK(jit::AtomicOperations::fetchAddSeqCst(p, B) == A);              \
  CHECK(*q == A + B);                                                   \
  *q = A;                                                               \
  CHECK(jit::AtomicOperations::fetchSubSeqCst(p, B) == A);              \
  CHECK(*q == A - B);                                                   \
  *q = A;                                                               \
  CHECK(jit::AtomicOperations::fetchAndSeqCst(p, B) == A);              \
  CHECK(*q == (A & B));                                                 \
  *q = A;                                                               \
  CHECK(jit::AtomicOperations::fetchOrSeqCst(p, B) == A);               \
  CHECK(*q == (A | B));                                                 \
  *q = A;                                                               \
  CHECK(jit::AtomicOperations::fetchXorSeqCst(p, B) == A);              \
  CHECK(*q == (A ^ B));                                                 \
  *q = A;                                                               \
  CHECK(jit::AtomicOperations::loadSafeWhenRacy(p) == A);               \
  jit::AtomicOperations::storeSafeWhenRacy(p, B);                       \
  CHECK(*q == B);                                                       \
  T* q2 = (T*)hidePointerValue((void*)atomicMem2);                      \
  SharedMem<T*> p2 =                                                    \
      SharedMem<T*>::shared((T*)hidePointerValue((void*)atomicMem2));   \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::memcpySafeWhenRacy(p2, p, sizeof(T));          \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::memcpySafeWhenRacy(p2, p.unwrap(), sizeof(T)); \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::memcpySafeWhenRacy(p2.unwrap(), p, sizeof(T)); \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::memmoveSafeWhenRacy(p2, p, sizeof(T));         \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::podCopySafeWhenRacy(p2, p, 1);                 \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::podMoveSafeWhenRacy(p2, p, 1);                 \
  CHECK(*q2 == A);                                                      \
  return true

BEGIN_REUSABLE_TEST(testAtomicOperationsU8) {
  const uint8_t A = 0xab;
  const uint8_t B = 0x37;
  ATOMIC_TESTS(uint8_t, A, B);
}
END_TEST(testAtomicOperationsU8)

BEGIN_REUSABLE_TEST(testAtomicOperationsI8) {
  const int8_t A = 0x3b;
  const int8_t B = 0x27;
  ATOMIC_TESTS(int8_t, A, B);
}
END_TEST(testAtomicOperationsI8)

BEGIN_REUSABLE_TEST(testAtomicOperationsU16) {
  const uint16_t A = 0xabdc;
  const uint16_t B = 0x3789;
  ATOMIC_TESTS(uint16_t, A, B);
}
END_TEST(testAtomicOperationsU16)

BEGIN_REUSABLE_TEST(testAtomicOperationsI16) {
  const int16_t A = 0x3bdc;
  const int16_t B = 0x2737;
  ATOMIC_TESTS(int16_t, A, B);
}
END_TEST(testAtomicOperationsI16)

BEGIN_REUSABLE_TEST(testAtomicOperationsU32) {
  const uint32_t A = 0xabdc0588;
  const uint32_t B = 0x37891942;
  ATOMIC_TESTS(uint32_t, A, B);
}
END_TEST(testAtomicOperationsU32)

BEGIN_REUSABLE_TEST(testAtomicOperationsI32) {
  const int32_t A = 0x3bdc0588;
  const int32_t B = 0x27371843;
  ATOMIC_TESTS(int32_t, A, B);
}
END_TEST(testAtomicOperationsI32)

BEGIN_REUSABLE_TEST(testAtomicOperationsU64) {
  if (!jit::AtomicOperations::hasAtomic8()) {
    return true;
  }

  const uint64_t A(0x9aadf00ddeadbeef);
  const uint64_t B(0x4eedbead1337f001);
  ATOMIC_TESTS(uint64_t, A, B);
}
END_TEST(testAtomicOperationsU64)

BEGIN_REUSABLE_TEST(testAtomicOperationsI64) {
  if (!jit::AtomicOperations::hasAtomic8()) {
    return true;
  }

  const int64_t A(0x2aadf00ddeadbeef);
  const int64_t B(0x4eedbead1337f001);
  ATOMIC_TESTS(int64_t, A, B);
}
END_TEST(testAtomicOperationsI64)

// T is the primitive float type we're testing, and A and B are references to
// constant bindings holding values of that type.
//
// Stay away from 0, NaN, infinities, and denormals.

#define ATOMIC_FLOAT_TESTS(T, A, B)                                     \
  T* q = (T*)hidePointerValue((void*)atomicMem);                        \
  *q = A;                                                               \
  SharedMem<T*> p =                                                     \
      SharedMem<T*>::shared((T*)hidePointerValue((T*)atomicMem));       \
  CHECK(*q == A);                                                       \
  CHECK(jit::AtomicOperations::loadSafeWhenRacy(p) == A);               \
  jit::AtomicOperations::storeSafeWhenRacy(p, B);                       \
  CHECK(*q == B);                                                       \
  T* q2 = (T*)hidePointerValue((void*)atomicMem2);                      \
  SharedMem<T*> p2 =                                                    \
      SharedMem<T*>::shared((T*)hidePointerValue((void*)atomicMem2));   \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::memcpySafeWhenRacy(p2, p, sizeof(T));          \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::memcpySafeWhenRacy(p2, p.unwrap(), sizeof(T)); \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::memcpySafeWhenRacy(p2.unwrap(), p, sizeof(T)); \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::memmoveSafeWhenRacy(p2, p, sizeof(T));         \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::podCopySafeWhenRacy(p2, p, 1);                 \
  CHECK(*q2 == A);                                                      \
  *q = A;                                                               \
  *q2 = B;                                                              \
  jit::AtomicOperations::podMoveSafeWhenRacy(p2, p, 1);                 \
  CHECK(*q2 == A);                                                      \
  return true

BEGIN_REUSABLE_TEST(testAtomicOperationsF32) {
  const float A(123.25);
  const float B(-987.75);
  ATOMIC_FLOAT_TESTS(float, A, B);
}
END_TEST(testAtomicOperationsF32)

BEGIN_REUSABLE_TEST(testAtomicOperationsF64) {
  const double A(123.25);
  const double B(-987.75);
  ATOMIC_FLOAT_TESTS(double, A, B);
}
END_TEST(testAtomicOperationsF64)

#define ATOMIC_CLAMPED_TESTS(T, A, B)                             \
  T* q = (T*)hidePointerValue((void*)atomicMem);                  \
  *q = A;                                                         \
  SharedMem<T*> p =                                               \
      SharedMem<T*>::shared((T*)hidePointerValue((T*)atomicMem)); \
  CHECK(*q == A);                                                 \
  CHECK(jit::AtomicOperations::loadSafeWhenRacy(p) == A);         \
  jit::AtomicOperations::storeSafeWhenRacy(p, B);                 \
  CHECK(*q == B);                                                 \
  return true

BEGIN_REUSABLE_TEST(testAtomicOperationsU8Clamped) {
  const uint8_clamped A(0xab);
  const uint8_clamped B(0x37);
  ATOMIC_CLAMPED_TESTS(uint8_clamped, A, B);
}
END_TEST(testAtomicOperationsU8Clamped)

#undef ATOMIC_TESTS
#undef ATOMIC_FLOAT_TESTS
#undef ATOMIC_CLAMPED_TESTS
