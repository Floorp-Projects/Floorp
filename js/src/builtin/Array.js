/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* ES5 15.4.4.14. */
function ArrayIndexOf(searchElement/*, fromIndex*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = TO_UINT32(O.length);

    /* Step 4. */
    if (len === 0)
        return -1;

    /* Step 5. */
    var n = arguments.length > 1 ? ToInteger(arguments[1]) : 0;

    /* Step 6. */
    if (n >= len)
        return -1;

    var k;
    /* Step 7. */
    if (n >= 0)
        k = n;
    /* Step 8. */
    else {
        /* Step a. */
        k = len + n;
        /* Step b. */
        if (k < 0)
            k = 0;
    }

    /* Step 9. */
    if (IsPackedArray(O)) {
        for (; k < len; k++) {
            if (O[k] === searchElement)
                return k;
        }
    } else {
        for (; k < len; k++) {
            if (k in O && O[k] === searchElement)
                return k;
        }
    }

    /* Step 10. */
    return -1;
}

function ArrayStaticIndexOf(list, searchElement/*, fromIndex*/) {
    if (arguments.length < 1)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.indexOf');
    var fromIndex = arguments.length > 2 ? arguments[2] : 0;
    return callFunction(ArrayIndexOf, list, searchElement, fromIndex);
}

/* ES5 15.4.4.15. */
function ArrayLastIndexOf(searchElement/*, fromIndex*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = TO_UINT32(O.length);

    /* Step 4. */
    if (len === 0)
        return -1;

    /* Step 5. */
    var n = arguments.length > 1 ? ToInteger(arguments[1]) : len - 1;

    /* Steps 6-7. */
    var k;
    if (n > len - 1)
        k = len - 1;
    else if (n < 0)
        k = len + n;
    else
        k = n;

    /* Step 8. */
    if (IsPackedArray(O)) {
        for (; k >= 0; k--) {
            if (O[k] === searchElement)
                return k;
        }
    } else {
        for (; k >= 0; k--) {
            if (k in O && O[k] === searchElement)
                return k;
        }
    }

    /* Step 9. */
    return -1;
}

function ArrayStaticLastIndexOf(list, searchElement/*, fromIndex*/) {
    if (arguments.length < 1)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.lastIndexOf');
    var fromIndex;
    if (arguments.length > 2) {
        fromIndex = arguments[2];
    } else {
        var O = ToObject(list);
        var len = TO_UINT32(O.length);
        fromIndex = len - 1;
    }
    return callFunction(ArrayLastIndexOf, list, searchElement, fromIndex);
}

/* ES5 15.4.4.16. */
function ArrayEvery(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = TO_UINT32(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.every');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 5. */
    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Steps 6-7. */
    /* Steps a (implicit), and d. */
    for (var k = 0; k < len; k++) {
        /* Step b */
        if (k in O) {
            /* Step c. */
            if (!callFunction(callbackfn, T, O[k], k, O))
                return false;
        }
    }

    /* Step 8. */
    return true;
}

function ArrayStaticEvery(list, callbackfn/*, thisArg*/) {
    if (arguments.length < 2)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.every');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    var T = arguments.length > 2 ? arguments[2] : void 0;
    return callFunction(ArrayEvery, list, callbackfn, T);
}

/* ES5 15.4.4.17. */
function ArraySome(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = TO_UINT32(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.some');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 5. */
    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Steps 6-7. */
    /* Steps a (implicit), and d. */
    for (var k = 0; k < len; k++) {
        /* Step b */
        if (k in O) {
            /* Step c. */
            if (callFunction(callbackfn, T, O[k], k, O))
                return true;
        }
    }

    /* Step 8. */
    return false;
}

function ArrayStaticSome(list, callbackfn/*, thisArg*/) {
    if (arguments.length < 2)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.some');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    var T = arguments.length > 2 ? arguments[2] : void 0;
    return callFunction(ArraySome, list, callbackfn, T);
}

/* ES5 15.4.4.18. */
function ArrayForEach(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = TO_UINT32(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.forEach');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 5. */
    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Steps 6-7. */
    /* Steps a (implicit), and d. */
    for (var k = 0; k < len; k++) {
        /* Step b */
        if (k in O) {
            /* Step c. */
            callFunction(callbackfn, T, O[k], k, O);
        }
    }

    /* Step 8. */
    return void 0;
}

/* ES5 15.4.4.19. */
function ArrayMap(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Step 2-3. */
    var len = TO_UINT32(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.map');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 5. */
    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Step 6. */
    var A = NewDenseArray(len);

    /* Step 7-8. */
    /* Step a (implicit), and d. */
    for (var k = 0; k < len; k++) {
        /* Step b */
        if (k in O) {
            /* Step c.i-iii. */
            var mappedValue = callFunction(callbackfn, T, O[k], k, O);
            // UnsafePutElements doesn't invoke setters, so we can use it here.
            UnsafePutElements(A, k, mappedValue);
        }
    }

    /* Step 9. */
    return A;
}

function ArrayStaticMap(list, callbackfn/*, thisArg*/) {
    if (arguments.length < 2)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.map');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    var T = arguments.length > 2 ? arguments[2] : void 0;
    return callFunction(ArrayMap, list, callbackfn, T);
}

function ArrayStaticForEach(list, callbackfn/*, thisArg*/) {
    if (arguments.length < 2)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.forEach');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    var T = arguments.length > 2 ? arguments[2] : void 0;
    callFunction(ArrayForEach, list, callbackfn, T);
}

/* ES5 15.4.4.21. */
function ArrayReduce(callbackfn/*, initialValue*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = TO_UINT32(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.reduce');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 6. */
    var k = 0;

    /* Steps 5, 7-8. */
    var accumulator;
    if (arguments.length > 1) {
        accumulator = arguments[1];
    } else {
        /* Step 5. */
        if (len === 0)
            ThrowError(JSMSG_EMPTY_ARRAY_REDUCE);
        if (IsPackedArray(O)) {
            accumulator = O[k++];
        } else {
            var kPresent = false;
            for (; k < len; k++) {
                if (k in O) {
                    accumulator = O[k];
                    kPresent = true;
                    k++;
                    break;
                }
            }
            if (!kPresent)
              ThrowError(JSMSG_EMPTY_ARRAY_REDUCE);
        }
    }

    /* Step 9. */
    /* Steps a (implicit), and d. */
    for (; k < len; k++) {
        /* Step b */
        if (k in O) {
            /* Step c. */
            accumulator = callbackfn(accumulator, O[k], k, O);
        }
    }

    /* Step 10. */
    return accumulator;
}

function ArrayStaticReduce(list, callbackfn) {
    if (arguments.length < 2)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.reduce');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    if (arguments.length > 2)
        return callFunction(ArrayReduce, list, callbackfn, arguments[2]);
    else
        return callFunction(ArrayReduce, list, callbackfn);
}

/* ES5 15.4.4.22. */
function ArrayReduceRight(callbackfn/*, initialValue*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = TO_UINT32(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.reduce');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 6. */
    var k = len - 1;

    /* Steps 5, 7-8. */
    var accumulator;
    if (arguments.length > 1) {
        accumulator = arguments[1];
    } else {
        /* Step 5. */
        if (len === 0)
            ThrowError(JSMSG_EMPTY_ARRAY_REDUCE);
        if (IsPackedArray(O)) {
            accumulator = O[k--];
        } else {
            var kPresent = false;
            for (; k >= 0; k--) {
                if (k in O) {
                    accumulator = O[k];
                    kPresent = true;
                    k--;
                    break;
                }
            }
            if (!kPresent)
                ThrowError(JSMSG_EMPTY_ARRAY_REDUCE);
        }
    }

    /* Step 9. */
    /* Steps a (implicit), and d. */
    for (; k >= 0; k--) {
        /* Step b */
        if (k in O) {
            /* Step c. */
            accumulator = callbackfn(accumulator, O[k], k, O);
        }
    }

    /* Step 10. */
    return accumulator;
}

function ArrayStaticReduceRight(list, callbackfn) {
    if (arguments.length < 2)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.reduceRight');
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    if (arguments.length > 2)
        return callFunction(ArrayReduceRight, list, callbackfn, arguments[2]);
    else
        return callFunction(ArrayReduceRight, list, callbackfn);
}

/* ES6 draft 2013-05-14 15.4.3.23. */
function ArrayFind(predicate/*, thisArg*/) {
    /* Steps 1-2. */
    var O = ToObject(this);

    /* Steps 3-5. */
    var len = ToInteger(O.length);

    /* Step 6. */
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.find');
    if (!IsCallable(predicate))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, predicate));

    /* Step 7. */
    var T = arguments.length > 1 ? arguments[1] : undefined;

    /* Steps 8-9. */
    /* Steps a (implicit), and e. */
    /* Note: this will hang in some corner-case situations, because of IEEE-754 numbers'
     * imprecision for large values. Example:
     * var obj = { 18014398509481984: true, length: 18014398509481988 };
     * Array.prototype.find.call(obj, () => true);
     */
    for (var k = 0; k < len; k++) {
        /* Steps b and c (implicit) */
        if (k in O) {
            /* Step d. */
            var kValue = O[k];
            if (callFunction(predicate, T, kValue, k, O))
                return kValue;
        }
    }

    /* Step 10. */
    return undefined;
}

/* ES6 draft 2013-05-14 15.4.3.23. */
function ArrayFindIndex(predicate/*, thisArg*/) {
    /* Steps 1-2. */
    var O = ToObject(this);

    /* Steps 3-5. */
    var len = ToInteger(O.length);

    /* Step 6. */
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.find');
    if (!IsCallable(predicate))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, predicate));

    /* Step 7. */
    var T = arguments.length > 1 ? arguments[1] : undefined;

    /* Steps 8-9. */
    /* Steps a (implicit), and e. */
    /* Note: this will hang in some corner-case situations, because of IEEE-754 numbers'
     * imprecision for large values. Example:
     * var obj = { 18014398509481984: true, length: 18014398509481988 };
     * Array.prototype.find.call(obj, () => true);
     */
    for (var k = 0; k < len; k++) {
        /* Steps b and c (implicit) */
        if (k in O) {
            /* Step d. */
            if (callFunction(predicate, T, O[k], k, O))
                return k;
        }
    }

    /* Step 10. */
    return -1;
}

#define ARRAY_ITERATOR_SLOT_ITERATED_OBJECT 0
#define ARRAY_ITERATOR_SLOT_NEXT_INDEX 1
#define ARRAY_ITERATOR_SLOT_ITEM_KIND 2

#define ITEM_KIND_VALUE 0
#define ITEM_KIND_KEY_AND_VALUE 1
#define ITEM_KIND_KEY 2

// ES6 draft specification, section 22.1.5.1, version 2013-09-05.
function CreateArrayIteratorAt(obj, kind, n) {
    var iteratedObject = ToObject(obj);
    var iterator = NewArrayIterator();
    UnsafeSetReservedSlot(iterator, ARRAY_ITERATOR_SLOT_ITERATED_OBJECT, iteratedObject);
    UnsafeSetReservedSlot(iterator, ARRAY_ITERATOR_SLOT_NEXT_INDEX, n);
    UnsafeSetReservedSlot(iterator, ARRAY_ITERATOR_SLOT_ITEM_KIND, kind);
    return iterator;
}
function CreateArrayIterator(obj, kind) {
    return CreateArrayIteratorAt(obj, kind, 0);
}

function ArrayIteratorIdentity() {
    return this;
}

function ArrayIteratorNext() {
    // FIXME: ArrayIterator prototype should not pass this test.  Bug 924059.
    if (!IsObject(this) || !IsArrayIterator(this))
        ThrowError(JSMSG_INCOMPATIBLE_METHOD, "ArrayIterator", "next", ToString(this));

    var a = UnsafeGetReservedSlot(this, ARRAY_ITERATOR_SLOT_ITERATED_OBJECT);
    var index = UnsafeGetReservedSlot(this, ARRAY_ITERATOR_SLOT_NEXT_INDEX);
    var itemKind = UnsafeGetReservedSlot(this, ARRAY_ITERATOR_SLOT_ITEM_KIND);

    // FIXME: This should be ToLength, which clamps at 2**53.  Bug 924058.
    if (index >= TO_UINT32(a.length)) {
        // When the above is changed to ToLength, use +1/0 here instead
        // of MAX_UINT32.
        UnsafeSetReservedSlot(this, ARRAY_ITERATOR_SLOT_NEXT_INDEX, 0xffffffff);
        return { value: undefined, done: true };
    }

    UnsafeSetReservedSlot(this, ARRAY_ITERATOR_SLOT_NEXT_INDEX, index + 1);

    if (itemKind === ITEM_KIND_VALUE)
        return { value: a[index], done: false };

    if (itemKind === ITEM_KIND_KEY_AND_VALUE) {
        var pair = NewDenseArray(2);
        pair[0] = index;
        pair[1] = a[index];
        return { value: pair, done : false };
    }

    assert(itemKind === ITEM_KIND_KEY, itemKind);
    return { value: index, done: false };
}

function ArrayValuesAt(n) {
    return CreateArrayIteratorAt(this, ITEM_KIND_VALUE, n);
}

function ArrayValues() {
    return CreateArrayIterator(this, ITEM_KIND_VALUE);
}

function ArrayEntries() {
    return CreateArrayIterator(this, ITEM_KIND_KEY_AND_VALUE);
}

function ArrayKeys() {
    return CreateArrayIterator(this, ITEM_KIND_KEY);
}

#ifdef ENABLE_PARALLEL_JS

/*
 * Strawman spec:
 *   http://wiki.ecmascript.org/doku.php?id=strawman:data_parallelism
 */

/**
 * Creates a new array by applying |func(e, i, self)| for each element |e|
 * with index |i|.
 */
function ArrayMapPar(func, mode) {
  if (!IsCallable(func))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, func));

  var self = ToObject(this);
  var length = self.length;
  var buffer = NewDenseArray(length);

  parallel: for (;;) {
    // Avoid parallel compilation if we are already nested in another
    // parallel section or the user told us not to parallelize. The
    // use of a for (;;) loop is working around some ion limitations:
    //
    // - Breaking out of named blocks does not currently work (bug 684384);
    // - Unreachable Code Elim. can't properly handle if (a && b) (bug 669796)
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    var slicesInfo = ComputeSlicesInfo(length);
    ForkJoin(mapThread, 0, slicesInfo.count, ForkJoinMode(mode));
    return buffer;
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  for (var i = 0; i < length; i++)
    UnsafePutElements(buffer, i, func(self[i], i, self));
  return buffer;

  function mapThread(workerId, sliceStart, sliceEnd) {
    var sliceShift = slicesInfo.shift;
    var sliceId;
    while (GET_SLICE(sliceStart, sliceEnd, sliceId)) {
      var indexStart = SLICE_START_INDEX(sliceShift, sliceId);
      var indexEnd = SLICE_END_INDEX(sliceShift, indexStart, length);
      for (var i = indexStart; i < indexEnd; i++)
        UnsafePutElements(buffer, i, func(self[i], i, self));
    }
    return sliceId;
  }

  return undefined;
}

/**
 * Reduces the elements in an array in parallel. Order is not fixed and |func|
 * is assumed to be associative.
 */
function ArrayReducePar(func, mode) {
  if (!IsCallable(func))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, func));

  var self = ToObject(this);
  var length = self.length;

  if (length === 0)
    ThrowError(JSMSG_EMPTY_ARRAY_REDUCE);

  parallel: for (;;) { // see ArrayMapPar() to explain why for(;;) etc
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    var slicesInfo = ComputeSlicesInfo(length);
    var numSlices = slicesInfo.count;
    var subreductions = NewDenseArray(numSlices);

    ForkJoin(reduceThread, 0, numSlices, ForkJoinMode(mode));

    var accumulator = subreductions[0];
    for (var i = 1; i < numSlices; i++)
      accumulator = func(accumulator, subreductions[i]);
    return accumulator;
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  var accumulator = self[0];
  for (var i = 1; i < length; i++)
    accumulator = func(accumulator, self[i]);
  return accumulator;

  function reduceThread(workerId, sliceStart, sliceEnd) {
    var sliceShift = slicesInfo.shift;
    var sliceId;
    while (GET_SLICE(sliceStart, sliceEnd, sliceId)) {
      var indexStart = SLICE_START_INDEX(sliceShift, sliceId);
      var indexEnd = SLICE_END_INDEX(sliceShift, indexStart, length);
      var accumulator = self[indexStart];
      for (var i = indexStart + 1; i < indexEnd; i++)
        accumulator = func(accumulator, self[i]);
      UnsafePutElements(subreductions, sliceId, accumulator);
    }
    return sliceId;
  }

  return undefined;
}

/**
 * Returns an array [s_0, ..., s_N] where |s_i| is equal to the reduction (as
 * per |reduce()|) of elements |0..i|. This is the generalization of partial
 * sum.
 */
function ArrayScanPar(func, mode) {
  if (!IsCallable(func))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, func));

  var self = ToObject(this);
  var length = self.length;

  if (length === 0)
    ThrowError(JSMSG_EMPTY_ARRAY_REDUCE);

  var buffer = NewDenseArray(length);

  parallel: for (;;) { // see ArrayMapPar() to explain why for(;;) etc
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    var slicesInfo = ComputeSlicesInfo(length);
    var numSlices = slicesInfo.count;

    // Scan slices individually (see comment on phase1()).
    ForkJoin(phase1, 0, numSlices, ForkJoinMode(mode));

    // Compute intermediates array (see comment on phase2()).
    var intermediates = [];
    var accumulator = buffer[finalElement(0)];
    ARRAY_PUSH(intermediates, accumulator);
    for (var i = 1; i < numSlices - 1; i++) {
      accumulator = func(accumulator, buffer[finalElement(i)]);
      ARRAY_PUSH(intermediates, accumulator);
    }

    // Complete each slice using intermediates array (see comment on phase2()).
    //
    // We start from slice 1 instead of 0 since there is no work to be done
    // for slice 0.
    if (numSlices > 1)
      ForkJoin(phase2, 1, numSlices, ForkJoinMode(mode));
    return buffer;
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  scan(self[0], 0, length);
  return buffer;

  function scan(accumulator, start, end) {
    UnsafePutElements(buffer, start, accumulator);
    for (var i = start + 1; i < end; i++) {
      accumulator = func(accumulator, self[i]);
      UnsafePutElements(buffer, i, accumulator);
    }
    return accumulator;
  }

  /**
   * In phase 1, we divide the source array into |numSlices| slices and
   * compute scan on each slice sequentially as if it were the entire
   * array. This function is responsible for computing one of those
   * slices.
   *
   * So, if we have an array [A,B,C,D,E,F,G,H,I], |numSlices === 3|,
   * and our function |func| is sum, then we would wind up computing a
   * result array like:
   *
   *     [A, A+B, A+B+C, D, D+E, D+E+F, G, G+H, G+H+I]
   *      ^~~~~~~~~~~~^  ^~~~~~~~~~~~^  ^~~~~~~~~~~~~^
   *      Slice 0        Slice 1        Slice 2
   *
   * Read on in phase2 to see what we do next!
   */
  function phase1(workerId, sliceStart, sliceEnd) {
    var sliceShift = slicesInfo.shift;
    var sliceId;
    while (GET_SLICE(sliceStart, sliceEnd, sliceId)) {
      var indexStart = SLICE_START_INDEX(sliceShift, sliceId);
      var indexEnd = SLICE_END_INDEX(sliceShift, indexStart, length);
      scan(self[indexStart], indexStart, indexEnd);
    }
    return sliceId;
  }

  /**
   * Computes the index of the final element computed by the slice |sliceId|.
   */
  function finalElement(sliceId) {
    var sliceShift = slicesInfo.shift;
    return SLICE_END_INDEX(sliceShift, SLICE_START_INDEX(sliceShift, sliceId), length) - 1;
  }

  /**
   * After computing the phase1 results, we compute an
   * |intermediates| array. |intermediates[i]| contains the result
   * of reducing the final value from each preceding slice j<i with
   * the final value of slice i. So, to continue our previous
   * example, the intermediates array would contain:
   *
   *   [A+B+C, (A+B+C)+(D+E+F), ((A+B+C)+(D+E+F))+(G+H+I)]
   *
   * Here I have used parenthesization to make clear the order of
   * evaluation in each case.
   *
   *   An aside: currently the intermediates array is computed
   *   sequentially. In principle, we could compute it in parallel,
   *   at the cost of doing duplicate work. This did not seem
   *   particularly advantageous to me, particularly as the number
   *   of slices is typically quite small (one per core), so I opted
   *   to just compute it sequentially.
   *
   * Phase 2 combines the results of phase1 with the intermediates
   * array to produce the final scan results. The idea is to
   * reiterate over each element S[i] in the slice |sliceId|, which
   * currently contains the result of reducing with S[0]...S[i]
   * (where S0 is the first thing in the slice), and combine that
   * with |intermediate[sliceId-1]|, which represents the result of
   * reducing everything in the input array prior to the slice.
   *
   * To continue with our example, in phase 1 we computed slice 1 to
   * be [D, D+E, D+E+F]. We will combine those results with
   * |intermediates[1-1]|, which is |A+B+C|, so that the final
   * result is [(A+B+C)+D, (A+B+C)+(D+E), (A+B+C)+(D+E+F)]. Again I
   * am using parentheses to clarify how these results were reduced.
   */
  function phase2(workerId, sliceStart, sliceEnd) {
    var sliceShift = slicesInfo.shift;
    var sliceId;
    while (GET_SLICE(sliceStart, sliceEnd, sliceId)) {
      var indexPos = SLICE_START_INDEX(sliceShift, sliceId);
      var indexEnd = SLICE_END_INDEX(sliceShift, indexPos, length);
      var intermediate = intermediates[sliceId - 1];
      for (; indexPos < indexEnd; indexPos++)
        UnsafePutElements(buffer, indexPos, func(intermediate, buffer[indexPos]));
    }
    return sliceId;
  }

  return undefined;
}

/**
 * |scatter()| redistributes the elements in the array into a new array.
 *
 * - targets: The index targets[i] indicates where the ith element
 *   should appear in the result.
 *
 * - defaultValue: what value to use for indices in the output array that
 *   are never targeted.
 *
 * - conflictFunc: The conflict function. Used to resolve what
 *   happens if two indices i and j in the source array are targeted
 *   as the same destination (i.e., targets[i] === targets[j]), then
 *   the final result is determined by applying func(targets[i],
 *   targets[j]). If no conflict function is provided, it is an error
 *   if targets[i] === targets[j].
 *
 * - length: length of the output array (if not specified, uses the
 *   length of the input).
 *
 * - mode: internal debugging specification.
 */
function ArrayScatterPar(targets, defaultValue, conflictFunc, length, mode) {
  if (conflictFunc && !IsCallable(conflictFunc))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(2, conflictFunc));

  var self = ToObject(this);

  if (length === undefined)
    length = self.length;

  var targetsLength = std_Math_min(targets.length, self.length);

  if (!IS_UINT32(targetsLength) || !IS_UINT32(length))
    ThrowError(JSMSG_BAD_ARRAY_LENGTH);

  // FIXME: Bug 965609: Find a better parallel startegy for scatter.

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  return seq();

  function seq() {
    var buffer = NewDenseArray(length);
    var conflicts = NewDenseArray(length);

    for (var i = 0; i < length; i++) {
      UnsafePutElements(buffer, i, defaultValue);
      UnsafePutElements(conflicts, i, false);
    }

    for (var i = 0; i < targetsLength; i++) {
      var x = self[i];
      var t = checkTarget(i, targets[i]);
      if (conflicts[t])
        x = collide(x, buffer[t]);

      UnsafePutElements(buffer, t, x, conflicts, t, true);
    }

    return buffer;
  }

  function collide(elem1, elem2) {
    if (conflictFunc === undefined)
      ThrowError(JSMSG_PAR_ARRAY_SCATTER_CONFLICT);

    return conflictFunc(elem1, elem2);
  }

  function checkTarget(i, t) {
    if (TO_INT32(t) !== t)
      ThrowError(JSMSG_PAR_ARRAY_SCATTER_BAD_TARGET, i);

    if (t < 0 || t >= length)
      ThrowError(JSMSG_PAR_ARRAY_SCATTER_BOUNDS);

    // It's not enough to return t, as -0 | 0 === -0.
    return TO_INT32(t);
  }

  return undefined;
}

/**
 * The filter operation applied in parallel.
 */
function ArrayFilterPar(func, mode) {
  if (!IsCallable(func))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, func));

  var self = ToObject(this);
  var length = self.length;

  parallel: for (;;) { // see ArrayMapPar() to explain why for(;;) etc
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    var slicesInfo = ComputeSlicesInfo(length);

    // Step 1. Compute which items from each slice of the result
    // buffer should be preserved. When we're done, we have an array
    // |survivors| containing a bitset for each chunk, indicating
    // which members of the chunk survived. We also keep an array
    // |counts| containing the total number of items that are being
    // preserved from within one slice.
    //
    // FIXME(bug 844890): Use typed arrays here.
    var numSlices = slicesInfo.count;
    var counts = NewDenseArray(numSlices);
    for (var i = 0; i < numSlices; i++)
      UnsafePutElements(counts, i, 0);
    var survivors = NewDenseArray(computeNum32BitChunks(length));
    ForkJoin(findSurvivorsThread, 0, numSlices, ForkJoinMode(mode));

    // Step 2. Compress the slices into one contiguous set.
    var count = 0;
    for (var i = 0; i < numSlices; i++)
      count += counts[i];
    var buffer = NewDenseArray(count);
    if (count > 0)
      ForkJoin(copySurvivorsThread, 0, numSlices, ForkJoinMode(mode));

    return buffer;
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  var buffer = [];
  for (var i = 0; i < length; i++) {
    var elem = self[i];
    if (func(elem, i, self))
      ARRAY_PUSH(buffer, elem);
  }
  return buffer;

  /**
   * Determine the number of 32-bit chunks for use with the survivors bitset.
   */
  function computeNum32BitChunks(length) {
    var chunks = length >>> 5;
    if (chunks << 5 === length)
      return chunks;
    return chunks + 1;
  }

  /**
   * As described above, our goal is to determine which items we
   * will preserve from a given slice. We do this one chunk at a
   * time. When we finish a chunk, we record our current count and
   * the next chunk sliceId, lest we should bail.
   */
  function findSurvivorsThread(workerId, sliceStart, sliceEnd) {
    var sliceShift = slicesInfo.shift;
    var sliceId;
    while (GET_SLICE(sliceStart, sliceEnd, sliceId)) {
      var count = 0;
      var indexStart = SLICE_START_INDEX(sliceShift, sliceId);
      var indexEnd = SLICE_END_INDEX(sliceShift, indexStart, length);
      var chunkStart = computeNum32BitChunks(indexStart);
      var chunkEnd = computeNum32BitChunks(indexEnd);
      for (var chunkPos = chunkStart; chunkPos < chunkEnd; chunkPos++, indexStart += 32) {
        var chunkBits = 0;
        for (var bit = 0, indexPos = indexStart; bit < 32 && indexPos < indexEnd; bit++, indexPos++) {
          var keep = !!func(self[indexPos], indexPos, self);
          chunkBits |= keep << bit;
          count += keep;
        }
        UnsafePutElements(survivors, chunkPos, chunkBits);
      }
      UnsafePutElements(counts, sliceId, count);
    }
    return sliceId;
  }

  function copySurvivorsThread(workerId, sliceStart, sliceEnd) {
    var sliceShift = slicesInfo.shift;
    var sliceId;
    while (GET_SLICE(sliceStart, sliceEnd, sliceId)) {
      // Copies the survivors from this slice into the correct position.
      // Note that this is an idempotent operation that does not invoke
      // user code. Therefore, we don't expect bailouts and make an
      // effort to proceed chunk by chunk or avoid duplicating work.

      // Total up the items preserved by previous slices.
      var total = 0;
      for (var i = 0; i < sliceId + 1; i++)
        total += counts[i];

      // Compute the final index we expect to write.
      var count = total - counts[sliceId];
      if (count === total)
        continue;

      // Iterate over the chunks assigned to us. Read the bitset for
      // each chunk. Copy values where a 1 appears until we have
      // written all the values that we expect to. We can just iterate
      // from 0...CHUNK_SIZE without fear of a truncated final chunk
      // because we are already checking for when count==total.
      var indexStart = SLICE_START_INDEX(sliceShift, sliceId);
      var indexEnd = SLICE_END_INDEX(sliceShift, indexStart, length);
      var chunkStart = computeNum32BitChunks(indexStart);
      var chunkEnd = computeNum32BitChunks(indexEnd);
      for (var chunkPos = chunkStart; chunkPos < chunkEnd; chunkPos++, indexStart += 32) {
        var chunkBits = survivors[chunkPos];
        if (!chunkBits)
          continue;

        for (var i = 0; i < 32; i++) {
          if (chunkBits & (1 << i)) {
            UnsafePutElements(buffer, count++, self[indexStart + i]);
            if (count === total)
              break;
          }
        }

        if (count == total)
          break;
      }
    }

    return sliceId;
  }

  return undefined;
}

/**
 * "Comprehension form": This is the function invoked for
 * |Array.{build,buildPar}(len, fn)| It creates a new array with length |len|
 * where index |i| is equal to |fn(i)|.
 *
 * The final |mode| argument is an internal argument used only during our
 * unit-testing.
 */
function ArrayStaticBuild(length, func) {
  if (!IS_UINT32(length))
    ThrowError(JSMSG_BAD_ARRAY_LENGTH);
  if (!IsCallable(func))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, func));

  var buffer = NewDenseArray(length);

  for (var i = 0; i < length; i++)
    UnsafePutElements(buffer, i, func(i));

  return buffer;
}

function ArrayStaticBuildPar(length, func, mode) {
  if (!IS_UINT32(length))
    ThrowError(JSMSG_BAD_ARRAY_LENGTH);
  if (!IsCallable(func))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(1, func));

  var buffer = NewDenseArray(length);

  parallel: for (;;) {
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    var slicesInfo = ComputeSlicesInfo(length);
    ForkJoin(constructThread, 0, slicesInfo.count, ForkJoinMode(mode));
    return buffer;
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  for (var i = 0; i < length; i++)
    UnsafePutElements(buffer, i, func(i));
  return buffer;

  function constructThread(workerId, sliceStart, sliceEnd) {
    var sliceShift = slicesInfo.shift;
    var sliceId;
    while (GET_SLICE(sliceStart, sliceEnd, sliceId)) {
      var indexStart = SLICE_START_INDEX(sliceShift, sliceId);
      var indexEnd = SLICE_END_INDEX(sliceShift, indexStart, length);
      for (var i = indexStart; i < indexEnd; i++)
        UnsafePutElements(buffer, i, func(i));
    }
    return sliceId;
  }

  return undefined;
}

/*
 * Mark the main operations as clone-at-callsite for better precision.
 * This is slightly overkill, as all that we really need is to
 * specialize to the receiver and the elemental function, but in
 * practice this is likely not so different, since element functions
 * are often used in exactly one place.
 */
SetScriptHints(ArrayMapPar,         { cloneAtCallsite: true });
SetScriptHints(ArrayReducePar,      { cloneAtCallsite: true });
SetScriptHints(ArrayScanPar,        { cloneAtCallsite: true });
SetScriptHints(ArrayScatterPar,     { cloneAtCallsite: true });
SetScriptHints(ArrayFilterPar,      { cloneAtCallsite: true });
SetScriptHints(ArrayStaticBuildPar, { cloneAtCallsite: true });

#endif /* ENABLE_PARALLEL_JS */
