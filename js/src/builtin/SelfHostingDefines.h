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

// NB: keep this in sync with the copy in vm/ArgumentsObject.h.
#define MAX_ARGS_LENGTH (500 * 1000)

// Spread non-empty argument list of up to 15 elements.
#define SPREAD(v, n) SPREAD_##n(v)
#define SPREAD_1(v) v[0]
#define SPREAD_2(v) SPREAD_1(v), v[1]
#define SPREAD_3(v) SPREAD_2(v), v[2]
#define SPREAD_4(v) SPREAD_3(v), v[3]
#define SPREAD_5(v) SPREAD_4(v), v[4]
#define SPREAD_6(v) SPREAD_5(v), v[5]
#define SPREAD_7(v) SPREAD_6(v), v[6]
#define SPREAD_8(v) SPREAD_7(v), v[7]
#define SPREAD_9(v) SPREAD_8(v), v[8]
#define SPREAD_10(v) SPREAD_9(v), v[9]
#define SPREAD_11(v) SPREAD_10(v), v[10]
#define SPREAD_12(v) SPREAD_11(v), v[11]
#define SPREAD_13(v) SPREAD_12(v), v[12]
#define SPREAD_14(v) SPREAD_13(v), v[13]
#define SPREAD_15(v) SPREAD_14(v), v[14]

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

#define ITEM_KIND_KEY 0
#define ITEM_KIND_VALUE 1
#define ITEM_KIND_KEY_AND_VALUE 2

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

#define MODULE_OBJECT_ENVIRONMENT_SLOT 2

#define MODULE_STATE_FAILED       0
#define MODULE_STATE_PARSED       1
#define MODULE_STATE_INSTANTIATED 2
#define MODULE_STATE_EVALUATED    3

#define STRING_GENERICS_CHAR_AT               0
#define STRING_GENERICS_CHAR_CODE_AT          1
#define STRING_GENERICS_CONCAT                2
#define STRING_GENERICS_ENDS_WITH             3
#define STRING_GENERICS_INCLUDES              4
#define STRING_GENERICS_INDEX_OF              5
#define STRING_GENERICS_LAST_INDEX_OF         6
#define STRING_GENERICS_LOCALE_COMPARE        7
#define STRING_GENERICS_MATCH                 8
#define STRING_GENERICS_NORMALIZE             9
#define STRING_GENERICS_REPLACE               10
#define STRING_GENERICS_SEARCH                11
#define STRING_GENERICS_SLICE                 12
#define STRING_GENERICS_SPLIT                 13
#define STRING_GENERICS_STARTS_WITH           14
#define STRING_GENERICS_SUBSTR                15
#define STRING_GENERICS_SUBSTRING             16
#define STRING_GENERICS_TO_LOWER_CASE         17
#define STRING_GENERICS_TO_LOCALE_LOWER_CASE  18
#define STRING_GENERICS_TO_LOCALE_UPPER_CASE  19
#define STRING_GENERICS_TO_UPPER_CASE         20
#define STRING_GENERICS_TRIM                  21
#define STRING_GENERICS_TRIM_LEFT             22
#define STRING_GENERICS_TRIM_RIGHT            23
#define STRING_GENERICS_METHODS_LIMIT         24

#endif
