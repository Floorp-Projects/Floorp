/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Specialized .h file to be used by both JS and C++ code.

#ifndef builtin_SelfHostingDefines_h
#define builtin_SelfHostingDefines_h

// Utility macros.
#define TO_INT32(x) ((x) | 0)
#define TO_UINT32(x) ((x) >>> 0)
#define IS_UINT32(x) ((x) >>> 0 === (x))
#define MAX_NUMERIC_INDEX 0x1fffffffffffff // == Math.pow(2, 53) - 1

// Unforgeable version of Function.prototype.apply.
#define FUN_APPLY(FUN, RECEIVER, ARGS) \
  callFunction(std_Function_apply, FUN, RECEIVER, ARGS)

// Property descriptor attributes.
#define ATTR_ENUMERABLE         0x01
#define ATTR_CONFIGURABLE       0x02
#define ATTR_WRITABLE           0x04

#define ATTR_NONENUMERABLE      0x08
#define ATTR_NONCONFIGURABLE    0x10
#define ATTR_NONWRITABLE        0x20

// The extended slot in which the self-hosted name for self-hosted builtins is
// stored.
#define LAZY_FUNCTION_NAME_SLOT 0

// The extended slot which contains a boolean value that indicates whether
// that the canonical name of the self-hosted builtins is set in self-hosted
// global. This slot is used only in debug build.
#define HAS_SELFHOSTED_CANONICAL_NAME_SLOT 0

// Stores the length for bound functions, so the .length property doesn't need
// to be resolved eagerly.
#define BOUND_FUN_LENGTH_SLOT 1

// Stores the private WeakMap slot used for WeakSets
#define WEAKSET_MAP_SLOT 0

#define ITERATOR_SLOT_TARGET 0
// Used for collection iterators.
#define ITERATOR_SLOT_RANGE 1
// Used for list, i.e. Array and String, iterators.
#define ITERATOR_SLOT_NEXT_INDEX 1
#define ITERATOR_SLOT_ITEM_KIND 2
// Used for ListIterator.
#define ITERATOR_SLOT_NEXT_METHOD 2

#define ITEM_KIND_KEY 0
#define ITEM_KIND_VALUE 1
#define ITEM_KIND_KEY_AND_VALUE 2

#define PROMISE_STATE_SLOT             0
#define PROMISE_RESULT_SLOT            1
#define PROMISE_FULFILL_REACTIONS_SLOT 2
#define PROMISE_REJECT_REACTIONS_SLOT  3
#define PROMISE_RESOLVE_FUNCTION_SLOT  4
#define PROMISE_REJECT_FUNCTION_SLOT   5
#define PROMISE_ALLOCATION_SITE_SLOT   6
#define PROMISE_RESOLUTION_SITE_SLOT   7
#define PROMISE_ALLOCATION_TIME_SLOT   8
#define PROMISE_RESOLUTION_TIME_SLOT   9
#define PROMISE_ID_SLOT               10
#define PROMISE_IS_HANDLED_SLOT       11

#define PROMISE_STATE_PENDING   0
#define PROMISE_STATE_FULFILLED 1
#define PROMISE_STATE_REJECTED  2

#define PROMISE_IS_HANDLED_STATE_HANDLED   0
#define PROMISE_IS_HANDLED_STATE_UNHANDLED 1
#define PROMISE_IS_HANDLED_STATE_REPORTED  2

#define PROMISE_HANDLER_IDENTITY 0
#define PROMISE_HANDLER_THROWER  1

#define PROMISE_REJECTION_TRACKER_OPERATION_REJECT false
#define PROMISE_REJECTION_TRACKER_OPERATION_HANDLE true

// NB: keep these in sync with the copy in jsfriendapi.h.
#define JSITER_OWNONLY    0x8   /* iterate over obj's own properties only */
#define JSITER_HIDDEN     0x10  /* also enumerate non-enumerable properties */
#define JSITER_SYMBOLS    0x20  /* also include symbol property keys */
#define JSITER_SYMBOLSONLY 0x40 /* exclude string property keys */


#define REGEXP_FLAGS_SLOT 2

#define REGEXP_IGNORECASE_FLAG  0x01
#define REGEXP_GLOBAL_FLAG      0x02
#define REGEXP_MULTILINE_FLAG   0x04
#define REGEXP_STICKY_FLAG      0x08
#define REGEXP_UNICODE_FLAG     0x10

#define MODULE_OBJECT_ENVIRONMENT_SLOT 3

#define MODULE_STATE_FAILED       0
#define MODULE_STATE_PARSED       1
#define MODULE_STATE_INSTANTIATED 2
#define MODULE_STATE_EVALUATED    3

#endif
