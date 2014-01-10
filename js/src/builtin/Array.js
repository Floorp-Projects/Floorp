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
function CreateArrayIterator(obj, kind) {
    var iteratedObject = ToObject(obj);
    var iterator = NewArrayIterator();
    UnsafeSetReservedSlot(iterator, ARRAY_ITERATOR_SLOT_ITERATED_OBJECT, iteratedObject);
    UnsafeSetReservedSlot(iterator, ARRAY_ITERATOR_SLOT_NEXT_INDEX, 0);
    UnsafeSetReservedSlot(iterator, ARRAY_ITERATOR_SLOT_ITEM_KIND, kind);
    return iterator;
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

/* The mode asserts options object. */
#define TRY_PARALLEL(MODE) \
  ((!MODE || MODE.mode !== "seq"))
#define ASSERT_SEQUENTIAL_IS_OK(MODE) \
  do { if (MODE) AssertSequentialIsOK(MODE) } while(false)

/* Slice array: see ComputeAllSliceBounds() */
#define SLICE_INFO(START, END) START, END, START, 0
#define SLICE_START(ID) ((ID << 2) + 0)
#define SLICE_END(ID)   ((ID << 2) + 1)
#define SLICE_POS(ID)   ((ID << 2) + 2)

/*
 * How many items at a time do we do recomp. for parallel execution.
 * Note that filter currently assumes that this is no greater than 32
 * in order to make use of a bitset.
 */
#define CHUNK_SHIFT 5
#define CHUNK_SIZE 32

/* Safe versions of ARRAY.push(ELEMENT) */
#define ARRAY_PUSH(ARRAY, ELEMENT) \
  callFunction(std_Array_push, ARRAY, ELEMENT);
#define ARRAY_SLICE(ARRAY, ELEMENT) \
  callFunction(std_Array_slice, ARRAY, ELEMENT);

/**
 * The ParallelSpew intrinsic is only defined in debug mode, so define a dummy
 * if debug is not on.
 */
#ifndef DEBUG
#define ParallelSpew(args)
#endif

/**
 * Determine the number of chunks of size CHUNK_SIZE;
 * note that the final chunk may be smaller than CHUNK_SIZE.
 */
function ComputeNumChunks(length) {
  var chunks = length >>> CHUNK_SHIFT;
  if (chunks << CHUNK_SHIFT === length)
    return chunks;
  return chunks + 1;
}

#define SLICES_PER_WORKER 8

/**
 * Compute the number of slices given an array length and the number of
 * chunks. Used in tandem with the workstealing scheduler.
 */
function ComputeNumSlices(workers, length, chunks) {
  if (length !== 0) {
    var slices = workers * SLICES_PER_WORKER;
    if (chunks < slices)
      return workers;
    return slices;
  }
  return workers;
}

/**
 * Computes the bounds for slice |sliceIndex| of |numItems| items,
 * assuming |numSlices| total slices. If numItems is not evenly
 * divisible by numSlices, then the final thread may have a bit of
 * extra work.
 */
function ComputeSliceBounds(numItems, sliceIndex, numSlices) {
  var sliceWidth = (numItems / numSlices) | 0;
  var extraChunks = (numItems % numSlices) | 0;

  var startIndex = sliceWidth * sliceIndex + std_Math_min(extraChunks, sliceIndex);
  var endIndex = startIndex + sliceWidth;
  if (sliceIndex < extraChunks)
    endIndex += 1;
  return [startIndex, endIndex];
}

/**
 * Divides |numItems| items amongst |numSlices| slices. The result
 * is an array containing multiple values per slice: the start
 * index, end index, current position, and some padding. The
 * current position is initially the same as the start index. To
 * access the values for a particular slice, use the macros
 * SLICE_START() and so forth.
 */
function ComputeAllSliceBounds(numItems, numSlices) {
  // FIXME(bug 844890): Use typed arrays here.
  var sliceWidth = (numItems / numSlices) | 0;
  var extraChunks = (numItems % numSlices) | 0;
  var counter = 0;
  var info = [];
  var i = 0;
  for (; i < extraChunks; i++) {
    ARRAY_PUSH(info, SLICE_INFO(counter, counter + sliceWidth + 1));
    counter += sliceWidth + 1;
  }
  for (; i < numSlices; i++) {
    ARRAY_PUSH(info, SLICE_INFO(counter, counter + sliceWidth));
    counter += sliceWidth;
  }
  return info;
}

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

    var chunks = ComputeNumChunks(length);
    var numWorkers = ForkJoinNumWorkers();
    var numSlices = ComputeNumSlices(numWorkers, length, chunks);
    var info = ComputeAllSliceBounds(chunks, numSlices);
    ForkJoin(mapSlice, ForkJoinMode(mode), numSlices);
    return buffer;
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  for (var i = 0; i < length; i++) {
    // Note: Unlike JS arrays, parallel arrays cannot have holes.
    var v = func(self[i], i, self);
    UnsafePutElements(buffer, i, v);
  }
  return buffer;

  function mapSlice(sliceId, warmup) {
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    if (warmup && chunkEnd > chunkPos + 1)
      chunkEnd = chunkPos + 1;

    while (chunkPos < chunkEnd) {
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);

      for (var i = indexStart; i < indexEnd; i++)
        UnsafePutElements(buffer, i, func(self[i], i, self));

      UnsafePutElements(info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd === info[SLICE_END(sliceId)];
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

    var chunks = ComputeNumChunks(length);
    var numWorkers = ForkJoinNumWorkers();
    if (chunks < numWorkers)
      break parallel;

    var numSlices = ComputeNumSlices(numWorkers, length, chunks);
    var info = ComputeAllSliceBounds(chunks, numSlices);
    var subreductions = NewDenseArray(numSlices);
    ForkJoin(reduceSlice, ForkJoinMode(mode), numSlices);
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

  function reduceSlice(sliceId, warmup) {
    var chunkStart = info[SLICE_START(sliceId)];
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    // (*) This function is carefully designed so that the warmup
    // (which executes with chunkStart === chunkPos) will execute all
    // potential loads and stores. In particular, the warmup run
    // processes two chunks rather than one. Moreover, it stores
    // accumulator into subreductions and then loads it again to
    // ensure that the load is executed during the warmup, as it will
    // certainly be executed during subsequent runs.

    if (warmup && chunkEnd > chunkPos + 2)
      chunkEnd = chunkPos + 2;

    if (chunkStart === chunkPos) {
      var indexPos = chunkStart << CHUNK_SHIFT;
      var accumulator = reduceChunk(self[indexPos], indexPos + 1, indexPos + CHUNK_SIZE);

      UnsafePutElements(subreductions, sliceId, accumulator, // see (*) above
                        info, SLICE_POS(sliceId), ++chunkPos);
    }

    var accumulator = subreductions[sliceId]; // see (*) above

    while (chunkPos < chunkEnd) {
      var indexPos = chunkPos << CHUNK_SHIFT;
      accumulator = reduceChunk(accumulator, indexPos, indexPos + CHUNK_SIZE);
      UnsafePutElements(subreductions, sliceId, accumulator, info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd === info[SLICE_END(sliceId)];
  }

  function reduceChunk(accumulator, from, to) {
    to = std_Math_min(to, length);
    for (var i = from; i < to; i++)
      accumulator = func(accumulator, self[i]);
    return accumulator;
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

    var chunks = ComputeNumChunks(length);
    var numWorkers = ForkJoinNumWorkers();
    if (chunks < numWorkers)
      break parallel;

    var numSlices = ComputeNumSlices(numWorkers, length, chunks);
    var info = ComputeAllSliceBounds(chunks, numSlices);

    // Scan slices individually (see comment on phase1()).
    ForkJoin(phase1, ForkJoinMode(mode), numSlices);

    // Compute intermediates array (see comment on phase2()).
    var intermediates = [];
    var accumulator = buffer[finalElement(0)];
    ARRAY_PUSH(intermediates, accumulator);
    for (var i = 1; i < numSlices - 1; i++) {
      accumulator = func(accumulator, buffer[finalElement(i)]);
      ARRAY_PUSH(intermediates, accumulator);
    }

    // Reset the current position information for each slice, but
    // convert from chunks to indices (see comment on phase2()).
    for (var i = 0; i < numSlices; i++) {
      info[SLICE_POS(i)] = info[SLICE_START(i)] << CHUNK_SHIFT;
      info[SLICE_END(i)] = info[SLICE_END(i)] << CHUNK_SHIFT;
    }
    info[SLICE_END(numSlices - 1)] = std_Math_min(info[SLICE_END(numSlices - 1)], length);

    // Complete each slice using intermediates array (see comment on phase2()).
    ForkJoin(phase2, ForkJoinMode(mode), numSlices);
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
  function phase1(sliceId, warmup) {
    var chunkStart = info[SLICE_START(sliceId)];
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    if (warmup && chunkEnd > chunkPos + 2)
      chunkEnd = chunkPos + 2;

    if (chunkPos === chunkStart) {
      // For the first chunk, the accumulator begins as the value in
      // the input at the start of the chunk.
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);
      scan(self[indexStart], indexStart, indexEnd);
      UnsafePutElements(info, SLICE_POS(sliceId), ++chunkPos);
    }

    while (chunkPos < chunkEnd) {
      // For each subsequent chunk, the accumulator begins as the
      // combination of the final value of prev chunk and the value in
      // the input at the start of this chunk. Note that this loop is
      // written as simple as possible, at the cost of an extra read
      // from the buffer per iteration.
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);
      var accumulator = func(buffer[indexStart - 1], self[indexStart]);
      scan(accumulator, indexStart, indexEnd);
      UnsafePutElements(info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd === info[SLICE_END(sliceId)];
  }

  /**
   * Computes the index of the final element computed by the slice |sliceId|.
   */
  function finalElement(sliceId) {
    var chunkEnd = info[SLICE_END(sliceId)]; // last chunk written by |sliceId| is endChunk - 1
    var indexStart = std_Math_min(chunkEnd << CHUNK_SHIFT, length);
    return indexStart - 1;
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
   *
   * SUBTLE: Because we are mutating |buffer| in place, we have to
   * be very careful about bailouts!  We cannot checkpoint a chunk
   * at a time as we do elsewhere because that assumes it is safe to
   * replay the portion of a chunk which was already processed.
   * Therefore, in this phase, we track the current position at an
   * index granularity, although this requires two memory writes per
   * index.
   */
  function phase2(sliceId, warmup) {
    if (sliceId === 0)
      return true; // No work to do for the 0th slice.

    var indexPos = info[SLICE_POS(sliceId)];
    var indexEnd = info[SLICE_END(sliceId)];

    if (warmup)
      indexEnd = std_Math_min(indexEnd, indexPos + CHUNK_SIZE);

    var intermediate = intermediates[sliceId - 1];
    for (; indexPos < indexEnd; indexPos++) {
      UnsafePutElements(buffer, indexPos, func(intermediate, buffer[indexPos]),
                        info, SLICE_POS(sliceId), indexPos + 1);
    }

    return indexEnd === info[SLICE_END(sliceId)];
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

  // The Divide-Scatter-Vector strategy:
  // 1. Slice |targets| array of indices ("scatter-vector") into N
  //    parts.
  // 2. Each of the N threads prepares an output buffer and a
  //    write-log.
  // 3. Each thread scatters according to one of the N parts into its
  //    own output buffer, tracking written indices in the write-log
  //    and resolving any resulting local collisions in parallel.
  // 4. Merge the parts (either in parallel or sequentially), using
  //    the write-logs as both the basis for finding merge-inputs and
  //    for detecting collisions.

  // The Divide-Output-Range strategy:
  // 1. Slice the range of indices [0..|length|-1] into N parts.
  //    Allocate a single shared output buffer of length |length|.
  // 2. Each of the N threads scans (the entirety of) the |targets|
  //    array, seeking occurrences of indices from that thread's part
  //    of the range, and writing the results into the shared output
  //    buffer.
  // 3. Since each thread has its own portion of the output range,
  //    every collision that occurs can be handled thread-locally.

  // SO:
  //
  // If |targets.length| >> |length|, Divide-Scatter-Vector seems like
  // a clear win over Divide-Output-Range, since for the latter, the
  // expense of redundantly scanning the |targets| will diminish the
  // gain from processing |length| in parallel, while for the former,
  // the total expense of building separate output buffers and the
  // merging post-process is small compared to the gain from
  // processing |targets| in parallel.
  //
  // If |targets.length| << |length|, then Divide-Output-Range seems
  // like it *could* win over Divide-Scatter-Vector. (But when is
  // |targets.length| << |length| or even |targets.length| < |length|?
  // Seems like an odd situation and an uncommon case at best.)
  //
  // The unanswered question is which strategy performs better when
  // |targets.length| approximately equals |length|, especially for
  // special cases like collision-free scatters and permutations.

  var targetsLength = std_Math_min(targets.length, self.length);

  if (!IS_UINT32(targetsLength) || !IS_UINT32(length))
    ThrowError(JSMSG_BAD_ARRAY_LENGTH);

  parallel: for (;;) { // see ArrayMapPar() to explain why for(;;) etc
    if (ShouldForceSequential())
      break parallel;
    if (!TRY_PARALLEL(mode))
      break parallel;

    if (forceDivideScatterVector())
      return parDivideScatterVector();
    else if (forceDivideOutputRange())
      return parDivideOutputRange();
    else if (conflictFunc === undefined && targetsLength < length)
      return parDivideOutputRange();
    return parDivideScatterVector();
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  return seq();

  function forceDivideScatterVector() {
    return mode && mode.strategy && mode.strategy === "divide-scatter-vector";
  }

  function forceDivideOutputRange() {
    return mode && mode.strategy && mode.strategy === "divide-output-range";
  }

  function collide(elem1, elem2) {
    if (conflictFunc === undefined)
      ThrowError(JSMSG_PAR_ARRAY_SCATTER_CONFLICT);

    return conflictFunc(elem1, elem2);
  }


  function parDivideOutputRange() {
    var chunks = ComputeNumChunks(targetsLength);
    var numSlices = ComputeNumSlices(ForkJoinNumWorkers(), length, chunks);
    var checkpoints = NewDenseArray(numSlices);
    for (var i = 0; i < numSlices; i++)
      UnsafePutElements(checkpoints, i, 0);

    var buffer = NewDenseArray(length);
    var conflicts = NewDenseArray(length);

    for (var i = 0; i < length; i++) {
      UnsafePutElements(buffer, i, defaultValue);
      UnsafePutElements(conflicts, i, false);
    }

    ForkJoin(fill, ForkJoinMode(mode), numSlices);
    return buffer;

    function fill(sliceId, warmup) {
      var indexPos = checkpoints[sliceId];
      var indexEnd = targetsLength;
      if (warmup)
        indexEnd = std_Math_min(indexEnd, indexPos + CHUNK_SIZE);

      // Range in the output for which we are responsible:
      var [outputStart, outputEnd] = ComputeSliceBounds(length, sliceId, numSlices);

      for (; indexPos < indexEnd; indexPos++) {
        var x = self[indexPos];
        var t = checkTarget(indexPos, targets[indexPos]);
        if (t < outputStart || t >= outputEnd)
          continue;
        if (conflicts[t])
          x = collide(x, buffer[t]);
        UnsafePutElements(buffer, t, x, conflicts, t, true, checkpoints, sliceId, indexPos + 1);
      }

      return indexEnd === targetsLength;
    }

    return undefined;
  }

  function parDivideScatterVector() {
    // Subtle: because we will be mutating the localBuffers and
    // conflict arrays in place, we can never replay an entry in the
    // target array for fear of inducing a conflict where none existed
    // before. Therefore, we must proceed not by chunks but rather by
    // individual indices.
    var numSlices = ComputeNumSlices(ForkJoinNumWorkers(), length, ComputeNumChunks(length));
    var info = ComputeAllSliceBounds(targetsLength, numSlices);

    // FIXME(bug 844890): Use typed arrays here.
    var localBuffers = NewDenseArray(numSlices);
    for (var i = 0; i < numSlices; i++)
      UnsafePutElements(localBuffers, i, NewDenseArray(length));
    var localConflicts = NewDenseArray(numSlices);
    for (var i = 0; i < numSlices; i++) {
      var conflicts_i = NewDenseArray(length);
      for (var j = 0; j < length; j++)
        UnsafePutElements(conflicts_i, j, false);
      UnsafePutElements(localConflicts, i, conflicts_i);
    }

    // Initialize the 0th buffer, which will become the output. For
    // the other buffers, we track which parts have been written to
    // using the conflict buffer so they do not need to be
    // initialized.
    var outputBuffer = localBuffers[0];
    for (var i = 0; i < length; i++)
      UnsafePutElements(outputBuffer, i, defaultValue);

    ForkJoin(fill, ForkJoinMode(mode), numSlices);
    mergeBuffers();
    return outputBuffer;

    function fill(sliceId, warmup) {
      var indexPos = info[SLICE_POS(sliceId)];
      var indexEnd = info[SLICE_END(sliceId)];
      if (warmup)
        indexEnd = std_Math_min(indexEnd, indexPos + CHUNK_SIZE);

      var localbuffer = localBuffers[sliceId];
      var conflicts = localConflicts[sliceId];
      while (indexPos < indexEnd) {
        var x = self[indexPos];
        var t = checkTarget(indexPos, targets[indexPos]);
        if (conflicts[t])
          x = collide(x, localbuffer[t]);
        UnsafePutElements(localbuffer, t, x, conflicts, t, true,
                          info, SLICE_POS(sliceId), ++indexPos);
      }

      return indexEnd === info[SLICE_END(sliceId)];
    }

    /**
     * Merge buffers 1..NUMSLICES into buffer 0. In principle, we could
     * parallelize the merge work as well. But for this first cut,
     * just do the merge sequentially.
     */
    function mergeBuffers() {
      var buffer = localBuffers[0];
      var conflicts = localConflicts[0];
      for (var i = 1; i < numSlices; i++) {
        var otherbuffer = localBuffers[i];
        var otherconflicts = localConflicts[i];
        for (var j = 0; j < length; j++) {
          if (otherconflicts[j]) {
            if (conflicts[j]) {
              buffer[j] = collide(otherbuffer[j], buffer[j]);
            } else {
              buffer[j] = otherbuffer[j];
              conflicts[j] = true;
            }
          }
        }
      }
    }

    return undefined;
  }

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

    var chunks = ComputeNumChunks(length);
    var numWorkers = ForkJoinNumWorkers();
    if (chunks < numWorkers * 2)
      break parallel;

    var numSlices = ComputeNumSlices(numWorkers, length, chunks);
    var info = ComputeAllSliceBounds(chunks, numSlices);

    // Step 1. Compute which items from each slice of the result
    // buffer should be preserved. When we're done, we have an array
    // |survivors| containing a bitset for each chunk, indicating
    // which members of the chunk survived. We also keep an array
    // |counts| containing the total number of items that are being
    // preserved from within one slice.
    //
    // FIXME(bug 844890): Use typed arrays here.
    var counts = NewDenseArray(numSlices);
    for (var i = 0; i < numSlices; i++)
      UnsafePutElements(counts, i, 0);
    var survivors = NewDenseArray(chunks);
    ForkJoin(findSurvivorsInSlice, ForkJoinMode(mode), numSlices);

    // Step 2. Compress the slices into one contiguous set.
    var count = 0;
    for (var i = 0; i < numSlices; i++)
      count += counts[i];
    var buffer = NewDenseArray(count);
    if (count > 0)
      ForkJoin(copySurvivorsInSlice, ForkJoinMode(mode), numSlices);

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
   * As described above, our goal is to determine which items we
   * will preserve from a given slice. We do this one chunk at a
   * time. When we finish a chunk, we record our current count and
   * the next chunk sliceId, lest we should bail.
   */
  function findSurvivorsInSlice(sliceId, warmup) {
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    if (warmup && chunkEnd > chunkPos)
      chunkEnd = chunkPos + 1;

    var count = counts[sliceId];
    while (chunkPos < chunkEnd) {
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);
      var chunkBits = 0;

      for (var bit = 0; indexStart + bit < indexEnd; bit++) {
        var keep = !!func(self[indexStart + bit], indexStart + bit, self);
        chunkBits |= keep << bit;
        count += keep;
      }

      UnsafePutElements(survivors, chunkPos, chunkBits,
                        counts, sliceId, count,
                        info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd === info[SLICE_END(sliceId)];
  }

  function copySurvivorsInSlice(sliceId, warmup) {
    // Copies the survivors from this slice into the correct position.
    // Note that this is an idempotent operation that does not invoke
    // user code. Therefore, we don't expect bailouts and make an
    // effort to proceed chunk by chunk or avoid duplicating work.

    // Total up the items preserved by previous slices.
    var count = 0;
    if (sliceId > 0) { // FIXME(#819219)---work around a bug in Ion's range checks
      for (var i = 0; i < sliceId; i++)
        count += counts[i];
    }

    // Compute the final index we expect to write.
    var total = count + counts[sliceId];
    if (count === total)
      return true;

    // Iterate over the chunks assigned to us. Read the bitset for
    // each chunk. Copy values where a 1 appears until we have
    // written all the values that we expect to. We can just iterate
    // from 0...CHUNK_SIZE without fear of a truncated final chunk
    // because we are already checking for when count==total.
    var chunkStart = info[SLICE_START(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];
    for (var chunk = chunkStart; chunk < chunkEnd; chunk++) {
      var chunkBits = survivors[chunk];
      if (!chunkBits)
        continue;

      var indexStart = chunk << CHUNK_SHIFT;
      for (var i = 0; i < CHUNK_SIZE; i++) {
        if (chunkBits & (1 << i)) {
          UnsafePutElements(buffer, count++, self[indexStart + i]);
          if (count === total)
            break;
        }
      }
    }

    return true;
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

    var chunks = ComputeNumChunks(length);
    var numWorkers = ForkJoinNumWorkers();
    var numSlices = ComputeNumSlices(numWorkers, length, chunks);
    var info = ComputeAllSliceBounds(chunks, numSlices);
    ForkJoin(constructSlice, ForkJoinMode(mode), numSlices);
    return buffer;
  }

  // Sequential fallback:
  ASSERT_SEQUENTIAL_IS_OK(mode);
  fill(0, length);
  return buffer;

  function constructSlice(sliceId, warmup) {
    var chunkPos = info[SLICE_POS(sliceId)];
    var chunkEnd = info[SLICE_END(sliceId)];

    if (warmup && chunkEnd > chunkPos)
      chunkEnd = chunkPos + 1;

    while (chunkPos < chunkEnd) {
      var indexStart = chunkPos << CHUNK_SHIFT;
      var indexEnd = std_Math_min(indexStart + CHUNK_SIZE, length);
      fill(indexStart, indexEnd);
      UnsafePutElements(info, SLICE_POS(sliceId), ++chunkPos);
    }

    return chunkEnd === info[SLICE_END(sliceId)];
  }

  function fill(indexStart, indexEnd) {
    for (var i = indexStart; i < indexEnd; i++)
      UnsafePutElements(buffer, i, func(i));
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
