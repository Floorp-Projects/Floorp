/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsstaticcheck_h___
#define jsstaticcheck_h___

#ifdef NS_STATIC_CHECKING
/*
 * Trigger a control flow check to make sure that code flows through label
 */
inline __attribute__ ((unused)) void MUST_FLOW_THROUGH(const char *label) {
}

/* avoid unused goto-label warnings */
#define MUST_FLOW_LABEL(label) goto label; label:

inline JS_FORCES_STACK void VOUCH_DOES_NOT_REQUIRE_STACK() {}

inline JS_FORCES_STACK void
JS_ASSERT_NOT_ON_TRACE(JSContext *cx)
{
    JS_ASSERT(!JS_ON_TRACE(cx));
}

#else
#define MUST_FLOW_THROUGH(label)            ((void) 0)
#define MUST_FLOW_LABEL(label)
#define VOUCH_DOES_NOT_REQUIRE_STACK()      ((void) 0)
#define JS_ASSERT_NOT_ON_TRACE(cx)          JS_ASSERT(!JS_ON_TRACE(cx))
#endif
#define VOUCH_HAVE_STACK                    VOUCH_DOES_NOT_REQUIRE_STACK

/* sixgill annotation defines */

/* Avoid name collision if included with other headers defining annotations. */
#ifndef HAVE_STATIC_ANNOTATIONS
#define HAVE_STATIC_ANNOTATIONS

#ifdef XGILL_PLUGIN

#define STATIC_PRECONDITION(COND)         __attribute__((precondition(#COND)))
#define STATIC_PRECONDITION_ASSUME(COND)  __attribute__((precondition_assume(#COND)))
#define STATIC_POSTCONDITION(COND)        __attribute__((postcondition(#COND)))
#define STATIC_POSTCONDITION_ASSUME(COND) __attribute__((postcondition_assume(#COND)))
#define STATIC_INVARIANT(COND)            __attribute__((invariant(#COND)))
#define STATIC_INVARIANT_ASSUME(COND)     __attribute__((invariant_assume(#COND)))

/* Used to make identifiers for assert/assume annotations in a function. */
#define STATIC_PASTE2(X,Y) X ## Y
#define STATIC_PASTE1(X,Y) STATIC_PASTE2(X,Y)

#define STATIC_ASSERT(COND)                          \
  JS_BEGIN_MACRO                                     \
    __attribute__((assert_static(#COND), unused))    \
    int STATIC_PASTE1(assert_static_, __COUNTER__);  \
  JS_END_MACRO

#define STATIC_ASSUME(COND)                          \
  JS_BEGIN_MACRO                                     \
    __attribute__((assume_static(#COND), unused))    \
    int STATIC_PASTE1(assume_static_, __COUNTER__);  \
  JS_END_MACRO

#define STATIC_ASSERT_RUNTIME(COND)                         \
  JS_BEGIN_MACRO                                            \
    __attribute__((assert_static_runtime(#COND), unused))   \
    int STATIC_PASTE1(assert_static_runtime_, __COUNTER__); \
  JS_END_MACRO

#else /* XGILL_PLUGIN */

#define STATIC_PRECONDITION(COND)          /* nothing */
#define STATIC_PRECONDITION_ASSUME(COND)   /* nothing */
#define STATIC_POSTCONDITION(COND)         /* nothing */
#define STATIC_POSTCONDITION_ASSUME(COND)  /* nothing */
#define STATIC_INVARIANT(COND)             /* nothing */
#define STATIC_INVARIANT_ASSUME(COND)      /* nothing */

#define STATIC_ASSERT(COND)          JS_BEGIN_MACRO /* nothing */ JS_END_MACRO
#define STATIC_ASSUME(COND)          JS_BEGIN_MACRO /* nothing */ JS_END_MACRO
#define STATIC_ASSERT_RUNTIME(COND)  JS_BEGIN_MACRO /* nothing */ JS_END_MACRO

#endif /* XGILL_PLUGIN */

#define STATIC_SKIP_INFERENCE STATIC_INVARIANT(skip_inference())

#endif /* HAVE_STATIC_ANNOTATIONS */

#endif /* jsstaticcheck_h___ */
