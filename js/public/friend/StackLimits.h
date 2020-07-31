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

namespace detail {

extern JS_FRIEND_API bool RunningWithTrustedPrincipals(JSContext* cx);

MOZ_ALWAYS_INLINE uintptr_t GetNativeStackLimitHelper(JSContext* cx,
                                                      JS::StackKind kind,
                                                      int extraAllowance) {
  uintptr_t limit = JS::RootingContext::get(cx)->nativeStackLimit[kind];
#if JS_STACK_GROWTH_DIRECTION > 0
  limit += extraAllowance;
#else
  limit -= extraAllowance;
#endif
  return limit;
}

}  // namespace detail

extern MOZ_COLD JS_FRIEND_API void ReportOverRecursed(JSContext* maybecx);

MOZ_ALWAYS_INLINE uintptr_t GetNativeStackLimit(JSContext* cx) {
  JS::StackKind kind = detail::RunningWithTrustedPrincipals(cx)
                           ? JS::StackForTrustedScript
                           : JS::StackForUntrustedScript;
  return detail::GetNativeStackLimitHelper(cx, kind, 0);
}

/*
 * These functions return |false| if we are close to using up the C++ stack.
 * They also report an overrecursion error, except for the DontReport variants.
 * The CheckSystemRecursionLimit variant gives us a little extra space so we
 * can ensure that crucial code is able to run. CheckRecursionLimitConservative
 * allows less space than any other check, including a safety buffer (as in, it
 * uses the untrusted limit and subtracts a little more from it).
 */

MOZ_ALWAYS_INLINE bool CheckRecursionLimit(JSContext* cx, uintptr_t limit) {
  int stackDummy;

  JS_STACK_OOM_POSSIBLY_FAIL_REPORT();

  if (!JS_CHECK_STACK_SIZE(limit, &stackDummy)) {
    ReportOverRecursed(cx);
    return false;
  }

  return true;
}

MOZ_ALWAYS_INLINE bool CheckRecursionLimitDontReport(uintptr_t limit) {
  int stackDummy;

  JS_STACK_OOM_POSSIBLY_FAIL();

  return JS_CHECK_STACK_SIZE(limit, &stackDummy);
}

MOZ_ALWAYS_INLINE bool CheckRecursionLimit(JSContext* cx) {
  JS_STACK_OOM_POSSIBLY_FAIL_REPORT();

  // GetNativeStackLimit(cx) is pretty slow because it has to do an uninlined
  // call to RunningWithTrustedPrincipals to determine which stack limit to
  // use. To work around this, check the untrusted limit first to avoid the
  // overhead in most cases.
  uintptr_t untrustedLimit =
      detail::GetNativeStackLimitHelper(cx, JS::StackForUntrustedScript, 0);
  if (MOZ_LIKELY(CheckRecursionLimitDontReport(untrustedLimit))) {
    return true;
  }
  return CheckRecursionLimit(cx, GetNativeStackLimit(cx));
}

MOZ_ALWAYS_INLINE bool CheckRecursionLimitDontReport(JSContext* cx) {
  return CheckRecursionLimitDontReport(GetNativeStackLimit(cx));
}

MOZ_ALWAYS_INLINE bool CheckRecursionLimitWithStackPointerDontReport(
    JSContext* cx, void* sp) {
  JS_STACK_OOM_POSSIBLY_FAIL();

  return JS_CHECK_STACK_SIZE(GetNativeStackLimit(cx), sp);
}

MOZ_ALWAYS_INLINE bool CheckRecursionLimitWithStackPointer(JSContext* cx,
                                                           void* sp) {
  JS_STACK_OOM_POSSIBLY_FAIL_REPORT();

  if (!JS_CHECK_STACK_SIZE(GetNativeStackLimit(cx), sp)) {
    ReportOverRecursed(cx);
    return false;
  }
  return true;
}

MOZ_ALWAYS_INLINE bool CheckRecursionLimitWithExtra(JSContext* cx,
                                                    size_t extra) {
  char stackDummy;
  char* sp = &stackDummy;
#if JS_STACK_GROWTH_DIRECTION > 0
  sp += extra;
#else
  sp -= extra;
#endif
  return CheckRecursionLimitWithStackPointer(cx, sp);
}

MOZ_ALWAYS_INLINE bool CheckSystemRecursionLimit(JSContext* cx) {
  return CheckRecursionLimit(
      cx, detail::GetNativeStackLimitHelper(cx, JS::StackForSystemCode, 0));
}

MOZ_ALWAYS_INLINE bool CheckRecursionLimitConservative(JSContext* cx) {
  return CheckRecursionLimit(
      cx, detail::GetNativeStackLimitHelper(cx, JS::StackForUntrustedScript,
                                            -1024 * int(sizeof(size_t))));
}

MOZ_ALWAYS_INLINE bool CheckRecursionLimitConservativeDontReport(
    JSContext* cx) {
  return CheckRecursionLimitDontReport(detail::GetNativeStackLimitHelper(
      cx, JS::StackForUntrustedScript, -1024 * int(sizeof(size_t))));
}

}  // namespace js

#endif  // js_friend_StackLimits_h
