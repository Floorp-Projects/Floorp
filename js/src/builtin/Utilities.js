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

/* Utility macros */
#define TO_INT32(x) ((x) | 0)
#define TO_UINT32(x) ((x) >>> 0)
#define IS_UINT32(x) ((x) >>> 0 === (x))

/* Assertions */
#ifdef DEBUG
#define assert(b, info) if (!(b)) AssertionFailed(info)
#else
#define assert(b, info)
#endif

/* cache built-in functions before applications can change them */
var std_isFinite = isFinite;
var std_isNaN = isNaN;
var std_Array_indexOf = ArrayIndexOf;
var std_Array_iterator = Array.prototype.iterator;
var std_Array_join = Array.prototype.join;
var std_Array_push = Array.prototype.push;
var std_Array_pop = Array.prototype.pop;
var std_Array_shift = Array.prototype.shift;
var std_Array_slice = Array.prototype.slice;
var std_Array_sort = Array.prototype.sort;
var std_Array_unshift = Array.prototype.unshift;
var std_Boolean_toString = Boolean.prototype.toString;
var Std_Date = Date;
var std_Date_now = Date.now;
var std_Date_valueOf = Date.prototype.valueOf;
var std_Function_bind = Function.prototype.bind;
var std_Function_apply = Function.prototype.apply;
var std_Math_floor = Math.floor;
var std_Math_max = Math.max;
var std_Math_min = Math.min;
var std_Math_imul = Math.imul;
var std_Math_log2 = Math.log2;
var std_Number_valueOf = Number.prototype.valueOf;
var std_Number_POSITIVE_INFINITY = Number.POSITIVE_INFINITY;
var std_Object_create = Object.create;
var std_Object_defineProperty = Object.defineProperty;
var std_Object_getOwnPropertyNames = Object.getOwnPropertyNames;
var std_Object_hasOwnProperty = Object.prototype.hasOwnProperty;
var std_RegExp_test = RegExp.prototype.test;
var Std_String = String;
var std_String_fromCharCode = String.fromCharCode;
var std_String_charCodeAt = String.prototype.charCodeAt;
var std_String_indexOf = String.prototype.indexOf;
var std_String_lastIndexOf = String.prototype.lastIndexOf;
var std_String_match = String.prototype.match;
var std_String_replace = String.prototype.replace;
var std_String_split = String.prototype.split;
var std_String_startsWith = String.prototype.startsWith;
var std_String_substring = String.prototype.substring;
var std_String_toLowerCase = String.prototype.toLowerCase;
var std_String_toUpperCase = String.prototype.toUpperCase;
var std_WeakMap = WeakMap;
var std_WeakMap_get = WeakMap.prototype.get;
var std_WeakMap_has = WeakMap.prototype.has;
var std_WeakMap_set = WeakMap.prototype.set;
var std_Map_has = Map.prototype.has;
var std_Set_has = Set.prototype.has;
var std_iterator = '@@iterator'; // FIXME: Change to be a symbol.
var std_StopIteration = StopIteration;
var std_Map_iterator = Map.prototype[std_iterator];
var std_Set_iterator = Set.prototype[std_iterator];
var std_Map_iterator_next = Object.getPrototypeOf(Map()[std_iterator]()).next;
var std_Set_iterator_next = Object.getPrototypeOf(Set()[std_iterator]()).next;

/* Safe versions of ARRAY.push(ELEMENT) */
#define ARRAY_PUSH(ARRAY, ELEMENT) \
  callFunction(std_Array_push, ARRAY, ELEMENT);
#define ARRAY_SLICE(ARRAY, ELEMENT) \
  callFunction(std_Array_slice, ARRAY, ELEMENT);

/********** Parallel JavaScript macros and so on **********/

#ifdef ENABLE_PARALLEL_JS

/* The mode asserts options object. */
#define TRY_PARALLEL(MODE) \
  ((!MODE || MODE.mode !== "seq"))
#define ASSERT_SEQUENTIAL_IS_OK(MODE) \
  do { if (MODE) AssertSequentialIsOK(MODE) } while(false)

/**
 * The ParallelSpew intrinsic is only defined in debug mode, so define a dummy
 * if debug is not on.
 */
#ifndef DEBUG
#define ParallelSpew(args)
#endif

#define MAX_SLICE_SHIFT 6
#define MAX_SLICE_SIZE 64
#define MAX_SLICES_PER_WORKER 8

/**
 * Macros to help compute the start and end indices of slices based on id. Use
 * with the object returned by ComputeSliceInfo.
 */
#define SLICE_START(info, id) \
    (id << info.shift)
#define SLICE_END(info, start, length) \
    std_Math_min(start + (1 << info.shift), length)
#define SLICE_COUNT(info) \
    info.statuses.length

/**
 * ForkJoinGetSlice acts as identity when we are not in a parallel section, so
 * pass in the next sequential value when we are in sequential mode. The
 * reason for this odd API is because intrinsics *need* to be called during
 * ForkJoin's warmup to fill the TI info.
 */
#define GET_SLICE(info, id) \
    ((id = ForkJoinGetSlice(InParallelSection() ? -1 : NextSequentialSliceId(info, -1))) >= 0)

#define SLICE_STATUS_DONE 1

/**
 * Macro to mark a slice as completed in the info object.
 */
#define MARK_SLICE_DONE(info, id) \
    UnsafePutElements(info.statuses, id, SLICE_STATUS_DONE)

#endif // ENABLE_PARALLEL_JS

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


/* Spec: ECMAScript Language Specification, 5.1 edition, 9.8 and 15.2.1.1 */
function ToString(v) {
    assert(arguments.length > 0, "__toString");
    return Std_String(v);
}


/* Spec: ECMAScript Language Specification, 5.1 edition, 9.10 */
function CheckObjectCoercible(v) {
    if (v === undefined || v === null)
        ThrowError(JSMSG_CANT_CONVERT_TO, ToString(v), "object");
}


/********** Various utility functions **********/


/** Returns true iff Type(v) is Object; see ES5 8.6. */
function IsObject(v) {
    // Watch out for |typeof null === "object"| as the most obvious pitfall.
    // But also be careful of SpiderMonkey's objects that emulate undefined
    // (i.e. |document.all|), which have bogus |typeof| behavior.  Detect
    // these objects using strict equality, which said bogosity doesn't affect.
    return (typeof v === "object" && v !== null) ||
           typeof v === "function" ||
           (typeof v === "undefined" && v !== undefined);
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
  return undefined;
}

#endif
