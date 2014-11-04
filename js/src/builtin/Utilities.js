/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*jshint bitwise: true, camelcase: false, curly: false, eqeqeq: true,
         es5: true, forin: true, immed: true, indent: 4, latedef: false,
         newcap: false, noarg: true, noempty: true, nonew: true,
         plusplus: false, quotmark: false, regexp: true, undef: true,
         unused: false, strict: false, trailing: true,
*/

/*global ToObject: false, ToInteger: false, IsCallable: false,
         ThrowError: false, AssertionFailed: false, SetScriptHints: false,
         MakeConstructible: false, DecompileArg: false,
         RuntimeDefaultLocale: false,
         ParallelDo: false, ParallelSlices: false, NewDenseArray: false,
         UnsafePutElements: false, ShouldForceSequential: false,
         ParallelTestsShouldPass: false,
         Dump: false,
         callFunction: false,
         TO_UINT32: false,
         JSMSG_NOT_FUNCTION: false, JSMSG_MISSING_FUN_ARG: false,
         JSMSG_EMPTY_ARRAY_REDUCE: false, JSMSG_CANT_CONVERT_TO: false,
*/

#include "SelfHostingDefines.h"

// All C++-implemented standard builtins library functions used in self-hosted
// code are installed via the std_functions JSFunctionSpec[] in
// SelfHosting.cpp.
//
// The few items below here are either self-hosted or installing them under a
// std_Foo name would require ugly contortions, so they just get aliased here.
var std_Array_indexOf = ArrayIndexOf;
var std_String_substring = String_substring;
// WeakMap is a bare constructor without properties or methods.
var std_WeakMap = WeakMap;
// StopIteration is a bare constructor without properties or methods.
var std_StopIteration = StopIteration;


/********** List specification type **********/


/* Spec: ECMAScript Language Specification, 5.1 edition, 8.8 */
function List() {}
{
  let ListProto = std_Object_create(null);
  ListProto.indexOf = std_Array_indexOf;
  ListProto.join = std_Array_join;
  ListProto.push = std_Array_push;
  ListProto.slice = std_Array_slice;
  ListProto.sort = std_Array_sort;
  MakeConstructible(List, ListProto);
}


/********** Record specification type **********/


/* Spec: ECMAScript Internationalization API Specification, draft, 5 */
function Record() {
    return std_Object_create(null);
}
MakeConstructible(Record, {});


/********** Abstract operations defined in ECMAScript Language Specification **********/


/* Spec: ECMAScript Language Specification, 5.1 edition, 8.12.6 and 11.8.7 */
function HasProperty(o, p) {
    return p in o;
}


/* Spec: ECMAScript Language Specification, 5.1 edition, 9.2 and 11.4.9 */
function ToBoolean(v) {
    return !!v;
}


/* Spec: ECMAScript Language Specification, 5.1 edition, 9.3 and 11.4.6 */
function ToNumber(v) {
    return +v;
}


/* Spec: ECMAScript Language Specification, 5.1 edition, 9.10 */
function CheckObjectCoercible(v) {
    if (v === undefined || v === null)
        ThrowError(JSMSG_CANT_CONVERT_TO, ToString(v), "object");
}

/* Spec: ECMAScript Draft, 6 edition May 22, 2014, 7.1.15 */
function ToLength(v) {
    v = ToInteger(v);

    if (v <= 0)
        return 0;

    // Math.pow(2, 53) - 1 = 0x1fffffffffffff
    return std_Math_min(v, 0x1fffffffffffff);
}

/********** Testing code **********/

#ifdef ENABLE_PARALLEL_JS

/**
 * Internal debugging tool: checks that the given `mode` permits
 * sequential execution
 */
function AssertSequentialIsOK(mode) {
  if (mode && mode.mode && mode.mode !== "seq" && ParallelTestsShouldPass())
    ThrowError(JSMSG_WRONG_VALUE, "parallel execution", "sequential was forced");
}

function ForkJoinMode(mode) {
  // WARNING: this must match the enum ForkJoinMode in ForkJoin.cpp
  if (!mode || !mode.mode) {
    return 0;
  } else if (mode.mode === "compile") {
    return 1;
  } else if (mode.mode === "par") {
    return 2;
  } else if (mode.mode === "recover") {
    return 3;
  } else if (mode.mode === "bailout") {
    return 4;
  }
  ThrowError(JSMSG_PAR_ARRAY_BAD_ARG);
}

#endif
