/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_IdForward_h
#define js_IdForward_h

#include <stddef.h>

// A jsid is an identifier for a property or method of an object which is
// either a 31-bit signed integer, interned string or object.
//
// Also, there is an additional jsid value, JSID_VOID, which does not occur in
// JS scripts but may be used to indicate the absence of a valid jsid.  A void
// jsid is not a valid id and only arises as an exceptional API return value,
// such as in JS_NextProperty. Embeddings must not pass JSID_VOID into JSAPI
// entry points expecting a jsid and do not need to handle JSID_VOID in hooks
// receiving a jsid except when explicitly noted in the API contract.
//
// A jsid is not implicitly convertible to or from a jsval; JS_ValueToId or
// JS_IdToValue must be used instead.
//
// In release builds, jsid is defined to be an integral type. This
// prevents many bugs from being caught at compile time. E.g.:
//
//  jsid id = ...
//  if (id)             // error
//    ...
//
//  size_t n = id;      // error
//
// To catch more errors, jsid is given a struct type in C++ debug builds.
// Struct assignment and (in C++) operator== allow correct code to be mostly
// oblivious to the change. This feature can be explicitly disabled in debug
// builds by defining JS_NO_JSVAL_JSID_STRUCT_TYPES.
//
// Note: if jsid was always a struct, we could just forward declare it in
// places where its declaration is needed.  But the fact that it's a typedef in
// non-debug builds prevents that.  So we have this file, which is morally
// equivalent to a forward declaration, and should be included by any file that
// uses jsid but doesn't need its definition.

#if defined(DEBUG) && !defined(JS_NO_JSVAL_JSID_STRUCT_TYPES)
# define JS_USE_JSID_STRUCT_TYPES
#endif

#ifdef JS_USE_JSID_STRUCT_TYPES
struct jsid;
#else
typedef ptrdiff_t jsid;
#endif

#endif /* js_IdForward_h */
