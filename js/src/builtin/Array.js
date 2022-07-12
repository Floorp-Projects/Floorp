/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* ES5 15.4.4.16. */
function ArrayEvery(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.every");
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 5. */
    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Steps 6-7. */
    /* Steps a (implicit), and d. */
    for (var k = 0; k < len; k++) {
        /* Step b */
        if (k in O) {
            /* Step c. */
            if (!callContentFunction(callbackfn, T, O[k], k, O))
                return false;
        }
    }

    /* Step 8. */
    return true;
}
// Inlining this enables inlining of the callback function.
SetIsInlinableLargeFunction(ArrayEvery);

/* ES5 15.4.4.17. */
function ArraySome(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.some");
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 5. */
    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Steps 6-7. */
    /* Steps a (implicit), and d. */
    for (var k = 0; k < len; k++) {
        /* Step b */
        if (k in O) {
            /* Step c. */
            if (callContentFunction(callbackfn, T, O[k], k, O))
                return true;
        }
    }

    /* Step 8. */
    return false;
}
// Inlining this enables inlining of the callback function.
SetIsInlinableLargeFunction(ArraySome);

// ES2018 draft rev 3bbc87cd1b9d3bf64c3e68ca2fe9c5a3f2c304c0
// 22.1.3.25 Array.prototype.sort ( comparefn )
function ArraySort(comparefn) {
    return SortArray(this, comparefn);
}

function SortArray(obj, comparefn) {
    // Step 1.
    if (comparefn !== undefined) {
        if (!IsCallable(comparefn))
            ThrowTypeError(JSMSG_BAD_SORT_ARG);
    }

    // Step 2.
    var O = ToObject(obj);

    // First try to sort the array in native code, if that fails, indicated by
    // returning |false| from ArrayNativeSort, sort it in self-hosted code.
    if (callFunction(ArrayNativeSort, O, comparefn))
        return O;

    // Step 3.
    var len = ToLength(O.length);

    if (len <= 1)
      return O;

    /* 22.1.3.25.1 Runtime Semantics: SortCompare( x, y ) */
    var wrappedCompareFn = comparefn;
    comparefn = function(x, y) {
        /* Steps 1-3. */
        if (x === undefined) {
            if (y === undefined)
                return 0;
           return 1;
        }
        if (y === undefined)
            return -1;

        /* Step 4.a. */
        var v = ToNumber(wrappedCompareFn(x, y));

        /* Step 4.b-c. */
        return v !== v ? 0 : v;
    };

    return MergeSort(O, len, comparefn);
}

/* ES5 15.4.4.18. */
function ArrayForEach(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.forEach");
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 5. */
    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Steps 6-7. */
    /* Steps a (implicit), and d. */
    for (var k = 0; k < len; k++) {
        /* Step b */
        if (k in O) {
            /* Step c. */
            callContentFunction(callbackfn, T, O[k], k, O);
        }
    }

    /* Step 8. */
    return void 0;
}
// Inlining this enables inlining of the callback function.
SetIsInlinableLargeFunction(ArrayForEach);

/* ES 2016 draft Mar 25, 2016 22.1.3.15. */
function ArrayMap(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Step 2. */
    var len = ToLength(O.length);

    /* Step 3. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.map");
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 4. */
    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Steps 5. */
    var A = ArraySpeciesCreate(O, len);

    /* Steps 6-7. */
    /* Steps 7.a (implicit), and 7.d. */
    for (var k = 0; k < len; k++) {
        /* Steps 7.b-c. */
        if (k in O) {
            /* Steps 7.c.i-iii. */
            var mappedValue = callContentFunction(callbackfn, T, O[k], k, O);
            DefineDataProperty(A, k, mappedValue);
        }
    }

    /* Step 8. */
    return A;
}
// Inlining this enables inlining of the callback function.
SetIsInlinableLargeFunction(ArrayMap);

/* ES 2016 draft Mar 25, 2016 22.1.3.7 Array.prototype.filter. */
function ArrayFilter(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Step 2. */
    var len = ToLength(O.length);

    /* Step 3. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.filter");
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 4. */
    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Step 5. */
    var A = ArraySpeciesCreate(O, 0);

    /* Steps 6-8. */
    /* Steps 8.a (implicit), and 8.d. */
    for (var k = 0, to = 0; k < len; k++) {
        /* Steps 8.b-c. */
        if (k in O) {
            /* Step 8.c.i. */
            var kValue = O[k];
            /* Step 8.c.ii. */
            var selected = callContentFunction(callbackfn, T, kValue, k, O);
            /* Step 8.c.iii. */
            if (selected)
                DefineDataProperty(A, to++, kValue);
        }
    }

    /* Step 9. */
    return A;
}

#ifdef NIGHTLY_BUILD
// Array Grouping proposal
//
// Array.prototype.groupBy
// https://tc39.es/proposal-array-grouping/#sec-array.prototype.groupby
function ArrayGroup(callbackfn/*, thisArg*/) {
    /* Step 1. Let O be ? ToObject(this value). */
    var O = ToObject(this);

    /* Step 2. Let len be ? LengthOfArrayLike(O). */
    var len = ToLength(O.length);

    /* Step 3. If IsCallable(callbackfn) is false, throw a TypeError exception. */
    if (!IsCallable(callbackfn)) {
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));
    }

    /* Step 5. Let groups be a new empty List. */
    var groups = new_List();

    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Steps 4, 6. */
    for (var k = 0; k < len; k++) {

        /* Skip Step 6.a. Let Pk be ! ToString(𝔽(k)).
         *
         * k is coerced into a string through the property access. */

        /* Step 6.b. Let kValue be ? Get(O, Pk). */
        var kValue = O[k];

        /* Step 6.c.
         * Let propertyKey be ? ToPropertyKey(
         *   ? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »)).
         */
        var propertyKey = TO_PROPERTY_KEY(
          callContentFunction(callbackfn, T, kValue, k, O)
        );

        /* Step 6.d. Perform ! AddValueToKeyedGroup(groups, propertyKey, kValue). */
        if (!groups[propertyKey]) {
            var elements = [ kValue ];
            DefineDataProperty(groups, propertyKey, elements);
        } else {
            var lenElements = groups[propertyKey].length;
            DefineDataProperty(groups[propertyKey], lenElements, kValue);
        }
    }

    /* Step 7. Let obj be ! OrdinaryObjectCreate(null). */
    var object = std_Object_create(null);

    /* Step 8. For each Record { [[Key]], [[Elements]] } g of groups, do
     *  a. Let elements be ! CreateArrayFromList(g.[[Elements]]).
     *  b. Perform ! CreateDataPropertyOrThrow(obj, g.[[Key]], elements).
     */
    for (var propertyKey in groups) {
        DefineDataProperty(object, propertyKey, groups[propertyKey])
    }

    /* Step 9. Return obj. */
    return object;
}

// Array Grouping proposal
//
// Array.prototype.groupToMap
// https://tc39.es/proposal-array-grouping/#sec-array.prototype.groupbymap
function ArrayGroupToMap(callbackfn/*, thisArg*/) {

    /* Step 1. Let O be ? ToObject(this value). */
    var O = ToObject(this);

    /* Step 2. Let len be ? LengthOfArrayLike(O). */
    var len = ToLength(O.length);

    /* Step 3.
     * If IsCallable(callbackfn) is false, throw a TypeError exception.
     */
    if (!IsCallable(callbackfn)) {
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));
    }

    /* Skipping Step 5. Let groups be a new empty List.
     *
     * Intermediate object isn't necessary as we have direct access
     * to the map constructor and set/get methods.
     */

    /* Step 7. Let map be ! Construct(%Map%). */
    var C = GetBuiltinConstructor("Map");
    var map = new C();

    var T = arguments.length > 1 ? arguments[1] : void 0;

    /* Combine Step 6. and Step 8.
     *
     * We have direct access to the map constructor and set/get methods.
     * We can treat these two loops as one, as there isn't a risk that user
     * polyfilling will impact the implementation.
     */
    for (var k = 0; k < len; k++) {
        /* Skipping Step 6.a. Let Pk be ! ToString(𝔽(k)).
         *
         * Value is coerced to String by property access in step 6.b.
         */

        /* Step 6.b. Let kValue be ? Get(O, Pk). */
        var kValue = O[k];

        /* Step 6.c.
         * Let key be ? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »).
         */
        var propertyKey = callContentFunction(callbackfn,T, kValue, k, O);

        /* Skipping Step 6.d. If key is -0𝔽, set key to +0𝔽.
         *
         * This step is performed by std_Map_set.
         */

        /* Step 8.c. Append entry as the last element of map.[[MapData]].
         *
         * We are not using an intermediate object to store the values.
         * So, this step applies it directly to the map object. Skips steps
         * 6.e (Perform ! AddValueToKeyedGroup(groups, key, kValue))
         * and 8.a-b as a result.
         */
        if (!callFunction(std_Map_get, map, propertyKey)) {
            var elements = [ kValue ];
            callFunction(std_Map_set, map, propertyKey, elements);
        } else {
            var elements = callFunction(std_Map_get, map, propertyKey);
            DefineDataProperty(elements, elements.length, kValue);
        }
    }

    /* Step 9. Return map. */
    return map;
}

#endif

/* ES5 15.4.4.21. */
function ArrayReduce(callbackfn/*, initialValue*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.reduce");
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 6. */
    var k = 0;

    /* Steps 5, 7-8. */
    var accumulator;
    if (arguments.length > 1) {
        accumulator = arguments[1];
    } else {
        /* Step 5. */
        // Add an explicit |throw| here and below to inform Ion that the
        // ThrowTypeError calls exit this function.
        if (len === 0)
            throw ThrowTypeError(JSMSG_EMPTY_ARRAY_REDUCE);

        // Use a |do-while| loop to let Ion know that the loop will definitely
        // be entered at least once. When Ion is then also able to inline the
        // |in| operator, it can optimize away the whole loop.
        var kPresent = false;
        do {
            if (k in O) {
                kPresent = true;
                break;
            }
        } while (++k < len);
        if (!kPresent)
          throw ThrowTypeError(JSMSG_EMPTY_ARRAY_REDUCE);

        // Moved outside of the loop to ensure the assignment is non-conditional.
        accumulator = O[k++];
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

/* ES5 15.4.4.22. */
function ArrayReduceRight(callbackfn/*, initialValue*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.reduce");
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 6. */
    var k = len - 1;

    /* Steps 5, 7-8. */
    var accumulator;
    if (arguments.length > 1) {
        accumulator = arguments[1];
    } else {
        /* Step 5. */
        // Add an explicit |throw| here and below to inform Ion that the
        // ThrowTypeError calls exit this function.
        if (len === 0)
            throw ThrowTypeError(JSMSG_EMPTY_ARRAY_REDUCE);

        // Use a |do-while| loop to let Ion know that the loop will definitely
        // be entered at least once. When Ion is then also able to inline the
        // |in| operator, it can optimize away the whole loop.
        var kPresent = false;
        do {
            if (k in O) {
                kPresent = true;
                break;
            }
        } while (--k >= 0);
        if (!kPresent)
            throw ThrowTypeError(JSMSG_EMPTY_ARRAY_REDUCE);

        // Moved outside of the loop to ensure the assignment is non-conditional.
        accumulator = O[k--];
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

/* ES6 draft 2013-05-14 15.4.3.23. */
function ArrayFind(predicate/*, thisArg*/) {
    /* Steps 1-2. */
    var O = ToObject(this);

    /* Steps 3-5. */
    var len = ToLength(O.length);

    /* Step 6. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.find");
    if (!IsCallable(predicate))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, predicate));

    /* Step 7. */
    var T = arguments.length > 1 ? arguments[1] : undefined;

    /* Steps 8-9. */
    /* Steps a (implicit), and g. */
    for (var k = 0; k < len; k++) {
        /* Steps a-c. */
        var kValue = O[k];
        /* Steps d-f. */
        if (callContentFunction(predicate, T, kValue, k, O))
            return kValue;
    }

    /* Step 10. */
    return undefined;
}

/* ES6 draft 2013-05-14 15.4.3.23. */
function ArrayFindIndex(predicate/*, thisArg*/) {
    /* Steps 1-2. */
    var O = ToObject(this);

    /* Steps 3-5. */
    var len = ToLength(O.length);

    /* Step 6. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.find");
    if (!IsCallable(predicate))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, predicate));

    /* Step 7. */
    var T = arguments.length > 1 ? arguments[1] : undefined;

    /* Steps 8-9. */
    /* Steps a (implicit), and g. */
    for (var k = 0; k < len; k++) {
        /* Steps a-f. */
        if (callContentFunction(predicate, T, O[k], k, O))
            return k;
    }

    /* Step 10. */
    return -1;
}

// ES2020 draft rev dc1e21c454bd316810be1c0e7af0131a2d7f38e9
// 22.1.3.3 Array.prototype.copyWithin ( target, start [ , end ] )
function ArrayCopyWithin(target, start, end = undefined) {
    // Step 1.
    var O = ToObject(this);

    // Step 2.
    var len = ToLength(O.length);

    // Step 3.
    var relativeTarget = ToInteger(target);

    // Step 4.
    var to = relativeTarget < 0 ? std_Math_max(len + relativeTarget, 0)
                                : std_Math_min(relativeTarget, len);

    // Step 5.
    var relativeStart = ToInteger(start);

    // Step 6.
    var from = relativeStart < 0 ? std_Math_max(len + relativeStart, 0)
                                 : std_Math_min(relativeStart, len);

    // Step 7.
    var relativeEnd = end === undefined ? len : ToInteger(end);

    // Step 8.
    var final = relativeEnd < 0 ? std_Math_max(len + relativeEnd, 0)
                                : std_Math_min(relativeEnd, len);

    // Step 9.
    var count = std_Math_min(final - from, len - to);

    // Steps 10-12.
    if (from < to && to < (from + count)) {
        // Steps 10.b-c.
        from = from + count - 1;
        to = to + count - 1;

        // Step 12.
        while (count > 0) {
            if (from in O)
                O[to] = O[from];
            else
                delete O[to];

            from--;
            to--;
            count--;
        }
    } else {
        // Step 12.
        while (count > 0) {
            if (from in O)
                O[to] = O[from];
            else
                delete O[to];

            from++;
            to++;
            count--;
        }
    }

    // Step 13.
    return O;
}

// ES2020 draft rev dc1e21c454bd316810be1c0e7af0131a2d7f38e9
// 22.1.3.6 Array.prototype.fill ( value [ , start [ , end ] ] )
function ArrayFill(value, start = 0, end = undefined) {
    // Step 1.
    var O = ToObject(this);

    // Step 2.
    var len = ToLength(O.length);

    // Step 3.
    var relativeStart = ToInteger(start);

    // Step 4.
    var k = relativeStart < 0
            ? std_Math_max(len + relativeStart, 0)
            : std_Math_min(relativeStart, len);

    // Step 5.
    var relativeEnd = end === undefined ? len : ToInteger(end);

    // Step 6.
    var final = relativeEnd < 0
                ? std_Math_max(len + relativeEnd, 0)
                : std_Math_min(relativeEnd, len);

    // Step 7.
    for (; k < final; k++) {
        O[k] = value;
    }

    // Step 8.
    return O;
}

// ES6 draft specification, section 22.1.5.1, version 2013-09-05.
function CreateArrayIterator(obj, kind) {
    var iteratedObject = ToObject(obj);
    var iterator = NewArrayIterator();
    UnsafeSetReservedSlot(iterator, ITERATOR_SLOT_TARGET, iteratedObject);
    UnsafeSetReservedSlot(iterator, ITERATOR_SLOT_NEXT_INDEX, 0);
    UnsafeSetReservedSlot(iterator, ITERATOR_SLOT_ITEM_KIND, kind);
    return iterator;
}

// ES6, 22.1.5.2.1
// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-%arrayiteratorprototype%.next
function ArrayIteratorNext() {
    // Step 1-3.
    var obj = this;
    if (!IsObject(obj) || (obj = GuardToArrayIterator(obj)) === null) {
        return callFunction(CallArrayIteratorMethodIfWrapped, this,
                            "ArrayIteratorNext");
    }

    // Step 4.
    var a = UnsafeGetReservedSlot(obj, ITERATOR_SLOT_TARGET);
    var result = { value: undefined, done: false };

    // Step 5.
    if (a === null) {
      result.done = true;
      return result;
    }

    // Step 6.
    // The index might not be an integer, so we have to do a generic get here.
    var index = UnsafeGetReservedSlot(obj, ITERATOR_SLOT_NEXT_INDEX);

    // Step 7.
    var itemKind = UnsafeGetInt32FromReservedSlot(obj, ITERATOR_SLOT_ITEM_KIND);

    // Step 8-9.
    var len;
    if (IsPossiblyWrappedTypedArray(a)) {
        len = PossiblyWrappedTypedArrayLength(a);

        // If the length is non-zero, the buffer can't be detached.
        if (len === 0) {
            if (PossiblyWrappedTypedArrayHasDetachedBuffer(a))
                ThrowTypeError(JSMSG_TYPED_ARRAY_DETACHED);
        }
    } else {
        len = ToLength(a.length);
    }

    // Step 10.
    if (index >= len) {
        UnsafeSetReservedSlot(obj, ITERATOR_SLOT_TARGET, null);
        result.done = true;
        return result;
    }

    // Step 11.
    UnsafeSetReservedSlot(obj, ITERATOR_SLOT_NEXT_INDEX, index + 1);

    // Step 16.
    if (itemKind === ITEM_KIND_VALUE) {
        result.value = a[index];
        return result;
    }

    // Step 13.
    if (itemKind === ITEM_KIND_KEY_AND_VALUE) {
        var pair = [index, a[index]];
        result.value = pair;
        return result;
    }

    // Step 12.
    assert(itemKind === ITEM_KIND_KEY, itemKind);
    result.value = index;
    return result;
}
// We want to inline this to do scalar replacement of the result object.
SetIsInlinableLargeFunction(ArrayIteratorNext);


// Uncloned functions with `$` prefix are allocated as extended function
// to store the original name in `SetCanonicalName`.
function $ArrayValues() {
    return CreateArrayIterator(this, ITEM_KIND_VALUE);
}
SetCanonicalName($ArrayValues, "values");

function ArrayEntries() {
    return CreateArrayIterator(this, ITEM_KIND_KEY_AND_VALUE);
}

function ArrayKeys() {
    return CreateArrayIterator(this, ITEM_KIND_KEY);
}

// ES 2017 draft 0f10dba4ad18de92d47d421f378233a2eae8f077 22.1.2.1
function ArrayFrom(items, mapfn = undefined, thisArg = undefined) {
    // Step 1.
    var C = this;

    // Steps 2-3.
    var mapping = mapfn !== undefined;
    if (mapping && !IsCallable(mapfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(1, mapfn));
    var T = thisArg;

    // Step 4.
    // Inlined: GetMethod, steps 1-2.
    var usingIterator = items[GetBuiltinSymbol("iterator")];

    // Step 5.
    // Inlined: GetMethod, step 3.
    if (usingIterator !== undefined && usingIterator !== null) {
        // Inlined: GetMethod, step 4.
        if (!IsCallable(usingIterator))
            ThrowTypeError(JSMSG_NOT_ITERABLE, DecompileArg(0, items));

        // Steps 5.a-b.
        var A = IsConstructor(C) ? new C() : [];

        // Step 5.c.
        var iterator = MakeIteratorWrapper(items, usingIterator);

        // Step 5.d.
        var k = 0;

        // Step 5.e
        for (var nextValue of allowContentIter(iterator)) {
            // Step 5.e.i.
            // Disabled for performance reason.  We won't hit this case on
            // normal array, since DefineDataProperty will throw before it.
            // We could hit this when |A| is a proxy and it ignores
            // |DefineDataProperty|, but it happens only after too long loop.
            /*
            if (k >= 0x1fffffffffffff)
                ThrowTypeError(JSMSG_TOO_LONG_ARRAY);
            */

            // Steps 5.e.vi-vii.
            var mappedValue = mapping ? callContentFunction(mapfn, T, nextValue, k) : nextValue;

            // Steps 5.e.ii (reordered), 5.e.viii.
            DefineDataProperty(A, k++, mappedValue);
        }

        // Step 5.e.iv.
        A.length = k;
        return A;
    }

    // Step 7 is an assertion: items is not an Iterator. Testing this is
    // literally the very last thing we did, so we don't assert here.

    // Steps 8-9.
    var arrayLike = ToObject(items);

    // Steps 10-11.
    var len = ToLength(arrayLike.length);

    // Steps 12-14.
    var A = IsConstructor(C) ? new C(len) : std_Array(len);

    // Steps 15-16.
    for (var k = 0; k < len; k++) {
        // Steps 16.a-c.
        var kValue = items[k];

        // Steps 16.d-e.
        var mappedValue = mapping ? callContentFunction(mapfn, T, kValue, k) : kValue;

        // Steps 16.f-g.
        DefineDataProperty(A, k, mappedValue);
    }

    // Steps 17-18.
    A.length = len;

    // Step 19.
    return A;
}

function MakeIteratorWrapper(items, method) {
    assert(IsCallable(method), "method argument is a function");

    // This function is not inlined in ArrayFrom, because function default
    // parameters combined with nested functions are currently not optimized
    // correctly.
    return {
        // Use a named function expression instead of a method definition, so
        // we don't create an inferred name for this function at runtime.
        [GetBuiltinSymbol("iterator")]: function IteratorMethod() {
            return callContentFunction(method, items);
        },
    };
}

// ES2015 22.1.3.27 Array.prototype.toString.
function ArrayToString() {
    // Steps 1-2.
    var array = ToObject(this);

    // Steps 3-4.
    var func = array.join;

    // Steps 5-6.
    if (!IsCallable(func))
        return callFunction(std_Object_toString, array);
    return callContentFunction(func, array);
}

// ES2017 draft rev f8a9be8ea4bd97237d176907a1e3080dce20c68f
// 22.1.3.27 Array.prototype.toLocaleString ([ reserved1 [ , reserved2 ] ])
// ES2017 Intl draft rev 78bbe7d1095f5ff3760ac4017ed366026e4cb276
// 13.4.1 Array.prototype.toLocaleString ([ locales [ , options ]])
function ArrayToLocaleString(locales, options) {
    // Step 1 (ToObject already performed in native code).
    assert(IsObject(this), "|this| should be an object");
    var array = this;

    // Step 2.
    var len = ToLength(array.length);

    // Step 4.
    if (len === 0)
        return "";

    // Step 5.
    var firstElement = array[0];

    // Steps 6-7.
    var R;
    if (firstElement === undefined || firstElement === null) {
        R = "";
    } else {
#if JS_HAS_INTL_API
        R = ToString(callContentFunction(firstElement.toLocaleString, firstElement, locales, options));
#else
        R = ToString(callContentFunction(firstElement.toLocaleString, firstElement));
#endif
    }

    // Step 3 (reordered).
    // We don't (yet?) implement locale-dependent separators.
    var separator = ",";

    // Steps 8-9.
    for (var k = 1; k < len; k++) {
        // Step 9.b.
        var nextElement = array[k];

        // Steps 9.a, 9.c-e.
        R += separator;
        if (!(nextElement === undefined || nextElement === null)) {
#if JS_HAS_INTL_API
            R += ToString(callContentFunction(nextElement.toLocaleString, nextElement, locales, options));
#else
            R += ToString(callContentFunction(nextElement.toLocaleString, nextElement));
#endif
        }
    }

    // Step 10.
    return R;
}

// ES 2016 draft Mar 25, 2016 22.1.2.5.
function $ArraySpecies() {
    // Step 1.
    return this;
}
SetCanonicalName($ArraySpecies, "get [Symbol.species]");

// ES 2016 draft Mar 25, 2016 9.4.2.3.
function ArraySpeciesCreate(originalArray, length) {
    // Step 1.
    assert(typeof length == "number", "length should be a number");
    assert(length >= 0, "length should be a non-negative number");

    // Step 2.
    // eslint-disable-next-line no-compare-neg-zero
    if (length === -0)
        length = 0;

    // Step 4, 6.
    if (!IsArray(originalArray))
        return std_Array(length);

    // Step 5.a.
    var C = originalArray.constructor;

    // Step 5.b.
    if (IsConstructor(C) && IsCrossRealmArrayConstructor(C))
        return std_Array(length);

    // Step 5.c.
    if (IsObject(C)) {
        // Step 5.c.i.
        C = C[GetBuiltinSymbol("species")];

        // Optimized path for an ordinary Array.
        if (C === GetBuiltinConstructor("Array"))
            return std_Array(length);

        // Step 5.c.ii.
        if (C === null)
            return std_Array(length);
    }

    // Step 6.
    if (C === undefined)
        return std_Array(length);

    // Step 7.
    if (!IsConstructor(C))
        ThrowTypeError(JSMSG_NOT_CONSTRUCTOR, "constructor property");

    // Step 8.
    return new C(length);
}

// ES 2017 draft (April 8, 2016) 22.1.3.1.1
function IsConcatSpreadable(O) {
    // Step 1.
    if (!IsObject(O)
#ifdef ENABLE_RECORD_TUPLE
        && !IsTuple(O)
#endif
    ) {
        return false;
    }

    // Step 2.
    var spreadable = O[GetBuiltinSymbol("isConcatSpreadable")];

    // Step 3.
    if (spreadable !== undefined)
        return ToBoolean(spreadable);

#ifdef ENABLE_RECORD_TUPLE
    if (IsTuple(O))
        return true;
#endif

    // Step 4.
    return IsArray(O);
}

// ES 2016 draft Mar 25, 2016 22.1.3.1.
// Note: Array.prototype.concat.length is 1.
function ArrayConcat(arg1) {
    // Step 1.
    var O = ToObject(this);

    // Step 2.
    var A = ArraySpeciesCreate(O, 0);

    // Step 3.
    var n = 0;

    // Step 4 (implicit in |arguments|).

    // Step 5.
    var i = 0, argsLen = arguments.length;

    // Step 5.a (first element).
    var E = O;

    var k, len;
    while (true) {
        // Steps 5.b-c.
        if (IsConcatSpreadable(E)) {
#ifdef ENABLE_RECORD_TUPLE
            // FIXME: spec bug - steps below expect that |E| is an object.
            E = ToObject(E);
#endif

            // Step 5.c.ii.
            len = ToLength(E.length);

            // Step 5.c.iii.
            if (n + len > MAX_NUMERIC_INDEX)
                ThrowTypeError(JSMSG_TOO_LONG_ARRAY);

            if (IsPackedArray(A) && IsPackedArray(E)) {
                // Step 5.c.i, 5.c.iv, and 5.c.iv.5.
                for (k = 0; k < len; k++) {
                    // Steps 5.c.iv.1-3.
                    // IsPackedArray(E) ensures that |k in E| is always true.
                    DefineDataProperty(A, n, E[k]);

                    // Step 5.c.iv.4.
                    n++;
                }
            } else {
                // Step 5.c.i, 5.c.iv, and 5.c.iv.5.
                for (k = 0; k < len; k++) {
                    // Steps 5.c.iv.1-3.
                    if (k in E)
                        DefineDataProperty(A, n, E[k]);

                    // Step 5.c.iv.4.
                    n++;
                }
            }
        } else {
            // Step 5.d.i.
            if (n >= MAX_NUMERIC_INDEX)
                ThrowTypeError(JSMSG_TOO_LONG_ARRAY);

            // Step 5.d.ii.
            DefineDataProperty(A, n, E);

            // Step 5.d.iii.
            n++;
        }

        if (i >= argsLen)
            break;
        // Step 5.a (subsequent elements).
        E = arguments[i];
        i++;
    }

    // Step 6.
    A.length = n;

    // Step 7.
    return A;
}

// ES2020 draft rev dc1e21c454bd316810be1c0e7af0131a2d7f38e9
// 22.1.3.11 Array.prototype.flatMap ( mapperFunction [ , thisArg ] )
function ArrayFlatMap(mapperFunction/*, thisArg*/) {
    // Step 1.
    var O = ToObject(this);

    // Step 2.
    var sourceLen = ToLength(O.length);

    // Step 3.
    if (!IsCallable(mapperFunction))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, mapperFunction));

    // Step 4.
    var T = arguments.length > 1 ? arguments[1] : undefined;

    // Step 5.
    var A = ArraySpeciesCreate(O, 0);

    // Step 6.
    FlattenIntoArray(A, O, sourceLen, 0, 1, mapperFunction, T);

    // Step 7.
    return A;
}

// ES2020 draft rev dc1e21c454bd316810be1c0e7af0131a2d7f38e9
// 22.1.3.10 Array.prototype.flat ( [ depth ] )
function ArrayFlat(/* depth */) {
     // Step 1.
    var O = ToObject(this);

    // Step 2.
    var sourceLen = ToLength(O.length);

    // Step 3.
    var depthNum = 1;

    // Step 4.
    if (arguments.length > 0 && arguments[0] !== undefined)
        depthNum = ToInteger(arguments[0]);

    // Step 5.
    var A = ArraySpeciesCreate(O, 0);

    // Step 6.
    FlattenIntoArray(A, O, sourceLen, 0, depthNum);

    // Step 7.
    return A;
}

// ES2020 draft rev dc1e21c454bd316810be1c0e7af0131a2d7f38e9
// 22.1.3.10.1 FlattenIntoArray ( target, source, sourceLen, start, depth [ , mapperFunction, thisArg ] )
function FlattenIntoArray(target, source, sourceLen, start, depth, mapperFunction, thisArg) {
    // Step 1.
    var targetIndex = start;

    // Steps 2-3.
    for (var sourceIndex = 0; sourceIndex < sourceLen; sourceIndex++) {
        // Steps 3.a-c.
        if (sourceIndex in source) {
            // Step 3.c.i.
            var element = source[sourceIndex];

            if (mapperFunction) {
                // Step 3.c.ii.1.
                assert(arguments.length === 7, "thisArg is present");

                // Step 3.c.ii.2.
                element = callContentFunction(mapperFunction, thisArg, element, sourceIndex, source);
            }

            // Step 3.c.iii.
            var shouldFlatten = false;

            // Step 3.c.iv.
            if (depth > 0) {
                // Step 3.c.iv.1.
                shouldFlatten = IsArray(element);
            }

            // Step 3.c.v.
            if (shouldFlatten) {
                // Step 3.c.v.1.
                var elementLen = ToLength(element.length);

                // Step 3.c.v.2.
                targetIndex = FlattenIntoArray(target, element, elementLen, targetIndex, depth - 1);
            } else {
                // Step 3.c.vi.1.
                if (targetIndex >= MAX_NUMERIC_INDEX)
                    ThrowTypeError(JSMSG_TOO_LONG_ARRAY);

                // Step 3.c.vi.2.
                DefineDataProperty(target, targetIndex, element);

                // Step 3.c.vi.3.
                targetIndex++;
            }
        }
    }

    // Step 4.
    return targetIndex;
}

// https://github.com/tc39/proposal-relative-indexing-method
// Array.prototype.at ( index )
function ArrayAt(index) {
     // Step 1.
    var O = ToObject(this);

    // Step 2.
    var len = ToLength(O.length);

    // Step 3.
    var relativeIndex = ToInteger(index);

    // Steps 4-5.
    var k;
    if (relativeIndex >= 0) {
        k = relativeIndex;
    } else {
        k = len + relativeIndex;
    }

    // Step 6.
    if (k < 0 || k >= len) {
        return undefined;
    }

    // Step 7.
    return O[k];
}
// This function is only barely too long for normal inlining.
SetIsInlinableLargeFunction(ArrayAt);

#ifdef ENABLE_CHANGE_ARRAY_BY_COPY

// https://github.com/tc39/proposal-change-array-by-copy
// Array.prototype.toReversed()
function ArrayToReversed() {

    /* Step 1. Let O be ? ToObject(this value). */
    var O = ToObject(this);

    /* Step 2. Let len be ? LengthOfArrayLike(O). */
    var len = ToLength(O.length);

    /* Step 3. Let A be ArrayCreate(𝔽(len)). */
    var A = std_Array(len);

    /* Step 4. Let k be 0. */
    /* Step 5. Repeat, while k < len, */
    for (var k = 0; k < len; k++) {
        /* Step 5a. Let from be ! ToString(𝔽(len - k - 1)). */
        var from = len - k - 1;
        /* Skip Step 5b. Let Pk be ToString(𝔽(k)).
         * k is coerced into a string through the property access. */
        /* Step 5c. Let fromValue be ? Get(O, from).  */
        var fromValue = O[from];
        /* Step 5d. Perform ! CreateDataPropertyOrThrow(A, 𝔽(k), fromValue. */
        DefineDataProperty(A, k, fromValue);
    }

    /* Step 6. Return A. */
    return A;
}

// https://github.com/tc39/proposal-change-array-by-copy
// Array.prototype.toSorted()
function ArrayToSorted(comparefn) {

    /* Step 1.  If comparefn is not undefined and IsCallable(comparefn) is
     * false, throw a TypeError exception.
     */
    if (comparefn !== undefined && !IsCallable(comparefn)) {
        ThrowTypeError(JSMSG_BAD_TOSORTED_ARG);
    }

    /* Step 2. Let O be ? ToObject(this value). */
    var O = ToObject(this);

    /* Step 3. Let len be ? LengthOfArrayLike(O) */
    var len = ToLength(O.length);

    /* Step 4. Let A be ? ArrayCreate(𝔽(len)). */
    var items = std_Array(len);

    /* We depart from steps 5-8 of the spec for performance reasons, as
     * following the spec would require copying the input array twice.
     * Instead, we create a new array that replaces holes with undefined,
     * and sort this array.
     */
    for (var k = 0; k < len; k++) {
        DefineDataProperty(items, k, O[k]);
    }

    SortArray(items, comparefn);

    /* Step 9. Return A */
    return items;
}

#endif

// https://github.com/tc39/proposal-array-find-from-last
// Array.prototype.findLast ( predicate, thisArg )
function ArrayFindLast(predicate/*, thisArg*/) {
    /* Steps 1. */
    var O = ToObject(this);

    /* Steps 2. */
    var len = ToLength(O.length);

    /* Step 3. */
    if (arguments.length === 0) {
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.findLast");
    }
    if (!IsCallable(predicate)) {
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, predicate));
    }

    var T = arguments.length > 1 ? arguments[1] : undefined;

    /* Step 4-5. */
    for (var k = len - 1; k >= 0; k--) {
        /* Steps 5.a-b. */
        var kValue = O[k];
        /* Steps 5.c-d. */
        if (callContentFunction(predicate, T, kValue, k, O)) {
            return kValue;
        }
    }

    /* Step 6. */
    return undefined;
}

// https://github.com/tc39/proposal-array-find-from-last
// Array.prototype.findLastIndex ( predicate, thisArg )
function ArrayFindLastIndex(predicate/*, thisArg*/) {
    /* Steps 1. */
    var O = ToObject(this);

    /* Steps 2. */
    var len = ToLength(O.length);

    /* Step 3. */
    if (arguments.length === 0) {
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Array.prototype.findLastIndex");
    }
    if (!IsCallable(predicate)) {
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, predicate));
    }

    var T = arguments.length > 1 ? arguments[1] : undefined;

    /* Step 4-5. */
    for (var k = len - 1; k >= 0; k--) {
        /* Steps 5.a-b. */
        var kValue = O[k];
        /* Steps 5.c-d. */
        if (callContentFunction(predicate, T, kValue, k, O)) {
            return k;
        }
    }

    /* Step 6. */
    return -1;
}
