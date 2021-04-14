/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_friend_StackLimits_h
#define js_friend_StackLimits_h

#include "mozilla/Attributes.h"  // MOZ_ALWAYS_INLINE, MOZ_COLD
#include "mozilla/Likely.h"      // MOZ_LIKELY

#include <stddef.h>  // size_t
#include <stdint.h>  // uintptr_t

#include "jstypes.h"  // JS_FRIEND_API

#include "js/HeapAPI.h"  // JS::StackKind, JS::StackForTrustedScript, JS::StackForUntrustedScript
#include "js/RootingAPI.h"  // JS::RootingContext
#include "js/Utility.h"  // JS_STACK_OOM_POSSIBLY_FAIL_REPORT, JS_STACK_OOM_POSSIBLY_FAIL

struct JS_PUBLIC_API JSContext;

#ifndef JS_STACK_GROWTH_DIRECTION
#  ifdef __hppa
#    define JS_STACK_GROWTH_DIRECTION (1)
#  else
#    define JS_STACK_GROWTH_DIRECTION (-1)
#  endif
#endif

#if JS_STACK_GROWTH_DIRECTION > 0
#  define JS_CHECK_STACK_SIZE(limit, sp) (MOZ_LIKELY((uintptr_t)(sp) < (limit)))
#else
#  define JS_CHECK_STACK_SIZE(limit, sp) (MOZ_LIKELY((uintptr_t)(sp) > (limit)))
#endif

namespace js {

// AutoCheckRecursionLimit can be used to check whether we're close to using up
// the C++ stack.
//
// Typical usage is like this:
//
//   AutoCheckRecursionLimit recursion(cx);
//   if (!recursion.check(cx)) {
//     return false;
//   }
//
// The check* functions return |false| if we are close to the stack limit.
// They also report an overrecursion error, except for the DontReport variants.
//
// The checkSystem variant gives us a little extra space so we can ensure that
// crucial code is able to run.
//
// checkConservative allows less space than any other check, including a safety
// buffer (as in, it uses the untrusted limit and subtracts a little more from
// it).
class MOZ_RAII AutoCheckRecursionLimit {
  [[nodiscard]] MOZ_ALWAYS_INLINE bool checkLimit(JSContext* cx,
                                                  uintptr_t limit) const;
  [[nodiscard]] MOZ_ALWAYS_INLINE bool checkLimitDontReport(
      uintptr_t limit) const;

  MOZ_ALWAYS_INLINE uintptr_t getStackLimit(JSContext* cx) const;
  MOZ_ALWAYS_INLINE uintptr_t getStackLimitHelper(JSContext* cx,
                                                  JS::StackKind kind,
                                                  int extraAllowance) const;

  JS_FRIEND_API bool runningWithTrustedPrincipals(JSContext* cx) const;

 public:
  explicit MOZ_ALWAYS_INLINE AutoCheckRecursionLimit(JSContext* cx) {}

  AutoCheckRecursionLimit(const AutoCheckRecursionLimit&) = delete;
  void operator=(const AutoCheckRecursionLimit&) = delete;

  [[nodiscard]] MOZ_ALWAYS_INLINE bool check(JSContext* cx) const;
  [[nodiscard]] MOZ_ALWAYS_INLINE bool checkDontReport(JSContext* cx) const;
  [[nodiscard]] MOZ_ALWAYS_INLINE bool checkWithExtra(JSContext* cx,
                                                      size_t extra) const;
  [[nodiscard]] MOZ_ALWAYS_INLINE bool checkWithStackPointerDontReport(
      JSContext* cx, void* sp) const;

  [[nodiscard]] MOZ_ALWAYS_INLINE bool checkConservative(JSContext* cx) const;
  [[nodiscard]] MOZ_ALWAYS_INLINE bool checkConservativeDontReport(
      JSContext* cx) const;

  [[nodiscard]] MOZ_ALWAYS_INLINE bool checkSystem(JSContext* cx) const;
};

extern MOZ_COLD JS_FRIEND_API void ReportOverRecursed(JSContext* maybecx);

MOZ_ALWAYS_INLINE uintptr_t
AutoCheckRecursionLimit::getStackLimit(JSContext* cx) const {
  JS::StackKind kind = runningWithTrustedPrincipals(cx)
                           ? JS::StackForTrustedScript
                           : JS::StackForUntrustedScript;
  return getStackLimitHelper(cx, kind, 0);
}

MOZ_ALWAYS_INLINE uintptr_t AutoCheckRecursionLimit::getStackLimitHelper(
    JSContext* cx, JS::StackKind kind, int extraAllowance) const {
  uintptr_t limit = JS::RootingContext::get(cx)->nativeStackLimit[kind];
#if JS_STACK_GROWTH_DIRECTION > 0
  limit += extraAllowance;
#else
  limit -= extraAllowance;
#endif
  return limit;
}

MOZ_ALWAYS_INLINE bool AutoCheckRecursionLimit::checkLimit(
    JSContext* cx, uintptr_t limit) const {
  int stackDummy;

  JS_STACK_OOM_POSSIBLY_FAIL_REPORT();

  if (!JS_CHECK_STACK_SIZE(limit, &stackDummy)) {
    ReportOverRecursed(cx);
    return false;
  }

  return true;
}

MOZ_ALWAYS_INLINE bool AutoCheckRecursionLimit::checkLimitDontReport(
    uintptr_t limit) const {
  int stackDummy;

  JS_STACK_OOM_POSSIBLY_FAIL();

  return JS_CHECK_STACK_SIZE(limit, &stackDummy);
}

MOZ_ALWAYS_INLINE bool AutoCheckRecursionLimit::check(JSContext* cx) const {
  JS_STACK_OOM_POSSIBLY_FAIL_REPORT();

  // getStackLimit(cx) is pretty slow because it has to do an uninlined
  // call to runningWithTrustedPrincipals to determine which stack limit to
  // use. To work around this, check the untrusted limit first to avoid the
  // overhead in most cases.
  uintptr_t untrustedLimit =
      getStackLimitHelper(cx, JS::StackForUntrustedScript, 0);
  if (MOZ_LIKELY(checkLimitDontReport(untrustedLimit))) {
    return true;
  }
  return checkLimit(cx, getStackLimit(cx));
}

MOZ_ALWAYS_INLINE bool AutoCheckRecursionLimit::checkDontReport(
    JSContext* cx) const {
  return checkLimitDontReport(getStackLimit(cx));
}

MOZ_ALWAYS_INLINE bool AutoCheckRecursionLimit::checkWithStackPointerDontReport(
    JSContext* cx, void* sp) const {
  JS_STACK_OOM_POSSIBLY_FAIL();

  return JS_CHECK_STACK_SIZE(getStackLimit(cx), sp);
}

MOZ_ALWAYS_INLINE bool AutoCheckRecursionLimit::checkWithExtra(
    JSContext* cx, size_t extra) const {
  char stackDummy;
  char* sp = &stackDummy;
#if JS_STACK_GROWTH_DIRECTION > 0
  sp += extra;
#else
  sp -= extra;
#endif

  JS_STACK_OOM_POSSIBLY_FAIL_REPORT();

  if (!JS_CHECK_STACK_SIZE(getStackLimit(cx), sp)) {
    ReportOverRecursed(cx);
    return false;
  }
  return true;
}

MOZ_ALWAYS_INLINE bool AutoCheckRecursionLimit::checkSystem(
    JSContext* cx) const {
  return checkLimit(cx, getStackLimitHelper(cx, JS::StackForSystemCode, 0));
}

MOZ_ALWAYS_INLINE bool AutoCheckRecursionLimit::checkConservative(
    JSContext* cx) const {
  return checkLimit(cx, getStackLimitHelper(cx, JS::StackForUntrustedScript,
                                            -1024 * int(sizeof(size_t))));
}

MOZ_ALWAYS_INLINE bool AutoCheckRecursionLimit::checkConservativeDontReport(
    JSContext* cx) const {
  return checkLimitDontReport(getStackLimitHelper(
      cx, JS::StackForUntrustedScript, -1024 * int(sizeof(size_t))));
}

}  // namespace js

#endif  // js_friend_StackLimits_h
