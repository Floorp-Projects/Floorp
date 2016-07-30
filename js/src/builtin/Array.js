/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* ES5 15.4.4.14. */
function ArrayIndexOf(searchElement/*, fromIndex*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (len === 0)
        return -1;

    /* Step 5.  Add zero to convert -0 to +0, per ES6 5.2. */
    var n = arguments.length > 1 ? ToInteger(arguments[1]) + 0 : 0;

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
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.indexOf');
    var fromIndex = arguments.length > 2 ? arguments[2] : 0;
    return callFunction(ArrayIndexOf, list, searchElement, fromIndex);
}

/* ES5 15.4.4.15. */
function ArrayLastIndexOf(searchElement/*, fromIndex*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (len === 0)
        return -1;

    /* Step 5.  Add zero to convert -0 to +0, per ES6 5.2. */
    var n = arguments.length > 1 ? ToInteger(arguments[1]) + 0 : len - 1;

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
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.lastIndexOf');
    var fromIndex;
    if (arguments.length > 2) {
        fromIndex = arguments[2];
    } else {
        var O = ToObject(list);
        var len = ToLength(O.length);
        fromIndex = len - 1;
    }
    return callFunction(ArrayLastIndexOf, list, searchElement, fromIndex);
}

/* ES5 15.4.4.16. */
function ArrayEvery(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.every');
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

function ArrayStaticEvery(list, callbackfn/*, thisArg*/) {
    if (arguments.length < 2)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.every');
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    var T = arguments.length > 2 ? arguments[2] : void 0;
    return callFunction(ArrayEvery, list, callbackfn, T);
}

/* ES5 15.4.4.17. */
function ArraySome(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.some');
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

function ArrayStaticSome(list, callbackfn/*, thisArg*/) {
    if (arguments.length < 2)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.some');
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    var T = arguments.length > 2 ? arguments[2] : void 0;
    return callFunction(ArraySome, list, callbackfn, T);
}

/* ES6 draft 2016-1-15 22.1.3.25 Array.prototype.sort (comparefn) */
function ArraySort(comparefn) {
    /* Step 1. */
    var O = ToObject(this);

    /* Step 2. */
    var len = ToLength(O.length);

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
    }

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
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.forEach');
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

function ArrayStaticForEach(list, callbackfn/*, thisArg*/) {
    if (arguments.length < 2)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.forEach');
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    var T = arguments.length > 2 ? arguments[2] : void 0;
    callFunction(ArrayForEach, list, callbackfn, T);
}

/* ES 2016 draft Mar 25, 2016 22.1.3.15. */
function ArrayMap(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Step 2. */
    var len = ToLength(O.length);

    /* Step 3. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.map');
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
            _DefineDataProperty(A, k, mappedValue);
        }
    }

    /* Step 8. */
    return A;
}

function ArrayStaticMap(list, callbackfn/*, thisArg*/) {
    if (arguments.length < 2)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.map');
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    var T = arguments.length > 2 ? arguments[2] : void 0;
    return callFunction(ArrayMap, list, callbackfn, T);
}

/* ES 2016 draft Mar 25, 2016 22.1.3.7 Array.prototype.filter. */
function ArrayFilter(callbackfn/*, thisArg*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Step 2. */
    var len = ToLength(O.length);

    /* Step 3. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.filter');
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
                _DefineDataProperty(A, to++, kValue);
        }
    }

    /* Step 9. */
    return A;
}

function ArrayStaticFilter(list, callbackfn/*, thisArg*/) {
    if (arguments.length < 2)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.filter');
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
    var T = arguments.length > 2 ? arguments[2] : void 0;
    return callFunction(ArrayFilter, list, callbackfn, T);
}

/* ES5 15.4.4.21. */
function ArrayReduce(callbackfn/*, initialValue*/) {
    /* Step 1. */
    var O = ToObject(this);

    /* Steps 2-3. */
    var len = ToLength(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.reduce');
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
        if (len === 0)
            ThrowTypeError(JSMSG_EMPTY_ARRAY_REDUCE);
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
              ThrowTypeError(JSMSG_EMPTY_ARRAY_REDUCE);
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
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.reduce');
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
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
    var len = ToLength(O.length);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.reduce');
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
        if (len === 0)
            ThrowTypeError(JSMSG_EMPTY_ARRAY_REDUCE);
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
                ThrowTypeError(JSMSG_EMPTY_ARRAY_REDUCE);
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
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.reduceRight');
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(1, callbackfn));
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
    var len = ToLength(O.length);

    /* Step 6. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.find');
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
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.prototype.find');
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

/* ES6 draft 2013-09-27 22.1.3.3. */
function ArrayCopyWithin(target, start, end = undefined) {
    /* Steps 1-2. */
    var O = ToObject(this);

    /* Steps 3-5. */
    var len = ToLength(O.length);

    /* Steps 6-8. */
    var relativeTarget = ToInteger(target);

    var to = relativeTarget < 0 ? std_Math_max(len + relativeTarget, 0)
                                : std_Math_min(relativeTarget, len);

    /* Steps 9-11. */
    var relativeStart = ToInteger(start);

    var from = relativeStart < 0 ? std_Math_max(len + relativeStart, 0)
                                 : std_Math_min(relativeStart, len);

    /* Steps 12-14. */
    var relativeEnd = end === undefined ? len
                                        : ToInteger(end);

    var final = relativeEnd < 0 ? std_Math_max(len + relativeEnd, 0)
                                : std_Math_min(relativeEnd, len);

    /* Step 15. */
    var count = std_Math_min(final - from, len - to);

    /* Steps 16-17. */
    if (from < to && to < (from + count)) {
        from = from + count - 1;
        to = to + count - 1;
        /* Step 18. */
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
        /* Step 18. */
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

    /* Step 19. */
    return O;
}

// ES6 draft 2014-04-05 22.1.3.6
function ArrayFill(value, start = 0, end = undefined) {
    // Steps 1-2.
    var O = ToObject(this);

    // Steps 3-5.
    var len = ToLength(O.length);

    // Steps 6-7.
    var relativeStart = ToInteger(start);

    // Step 8.
    var k = relativeStart < 0
            ? std_Math_max(len + relativeStart, 0)
            : std_Math_min(relativeStart, len);

    // Steps 9-10.
    var relativeEnd = end === undefined ? len : ToInteger(end);

    // Step 11.
    var final = relativeEnd < 0
                ? std_Math_max(len + relativeEnd, 0)
                : std_Math_min(relativeEnd, len);

    // Step 12.
    for (; k < final; k++) {
        O[k] = value;
    }

    // Step 13.
    return O;
}

// Proposed for ES7:
// https://github.com/tc39/Array.prototype.includes/blob/7c023c19a0/spec.md
function ArrayIncludes(searchElement, fromIndex = 0) {
    // Steps 1-2.
    var O = ToObject(this);

    // Steps 3-4.
    var len = ToLength(O.length);

    // Step 5.
    if (len === 0)
        return false;

    // Steps 6-7.
    var n = ToInteger(fromIndex);

    // Step 8.
    var k;
    if (n >= 0) {
        k = n;
    }
    // Step 9.
    else {
        // Step a.
        k = len + n;
        // Step b.
        if (k < 0)
            k = 0;
    }

    // Step 10.
    while (k < len) {
        // Steps a-c.
        if (SameValueZero(searchElement, O[k]))
            return true;

        // Step d.
        k++;
    }

    // Step 11.
    return false;
}

// ES6 draft specification, section 22.1.5.1, version 2013-09-05.
function CreateArrayIteratorAt(obj, kind, n) {
    var iteratedObject = ToObject(obj);
    var iterator = NewArrayIterator();
    UnsafeSetReservedSlot(iterator, ITERATOR_SLOT_TARGET, iteratedObject);
    UnsafeSetReservedSlot(iterator, ITERATOR_SLOT_NEXT_INDEX, n);
    UnsafeSetReservedSlot(iterator, ITERATOR_SLOT_ITEM_KIND, kind);
    return iterator;
}
function CreateArrayIterator(obj, kind) {
    return CreateArrayIteratorAt(obj, kind, 0);
}

// ES6, 22.1.5.2.1
// http://www.ecma-international.org/ecma-262/6.0/index.html#sec-%arrayiteratorprototype%.next
function ArrayIteratorNext() {
    // Step 1-3.
    if (!IsObject(this) || !IsArrayIterator(this)) {
        return callFunction(CallArrayIteratorMethodIfWrapped, this,
                            "ArrayIteratorNext");
    }

    // Step 4.
    var a = UnsafeGetReservedSlot(this, ITERATOR_SLOT_TARGET);
    var result = { value: undefined, done: false };

    // Step 5.
    if (a === null) {
      result.done = true;
      return result;
    }

    // Step 6.
    // The index might not be an integer, so we have to do a generic get here.
    var index = UnsafeGetReservedSlot(this, ITERATOR_SLOT_NEXT_INDEX);

    // Step 7.
    var itemKind = UnsafeGetInt32FromReservedSlot(this, ITERATOR_SLOT_ITEM_KIND);

    // Step 8-9.
    var len = IsPossiblyWrappedTypedArray(a)
              ? PossiblyWrappedTypedArrayLength(a)
              : ToLength(a.length);

    // Step 10.
    if (index >= len) {
        UnsafeSetReservedSlot(this, ITERATOR_SLOT_TARGET, null);
        result.done = true;
        return result;
    }

    // Step 11.
    UnsafeSetReservedSlot(this, ITERATOR_SLOT_NEXT_INDEX, index + 1);

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

function ArrayValuesAt(n) {
    return CreateArrayIteratorAt(this, ITEM_KIND_VALUE, n);
}

function ArrayValues() {
    return CreateArrayIterator(this, ITEM_KIND_VALUE);
}
_SetCanonicalName(ArrayValues, "values");

function ArrayEntries() {
    return CreateArrayIterator(this, ITEM_KIND_KEY_AND_VALUE);
}

function ArrayKeys() {
    return CreateArrayIterator(this, ITEM_KIND_KEY);
}

// ES6 draft rev31 (2015/01/15) 22.1.2.1 Array.from(source[, mapfn[, thisArg]]).
function ArrayFrom(items, mapfn=undefined, thisArg=undefined) {
    // Step 1.
    var C = this;

    // Steps 2-3.
    var mapping = mapfn !== undefined;
    if (mapping && !IsCallable(mapfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(1, mapfn));
    var T = thisArg;

    // Steps 4-5.
    var usingIterator = GetMethod(items, std_iterator);

    // Step 6.
    if (usingIterator !== undefined) {
        // Steps 6.a-c.
        var A = IsConstructor(C) ? new C() : [];

        // Steps 6.d-e.
        var iterator = GetIterator(items, usingIterator);

        // Step 6.f.
        var k = 0;

        // Step 6.g.
        // These steps cannot be implemented using a for-of loop.
        // See <https://bugs.ecmascript.org/show_bug.cgi?id=2883>.
        while (true) {
            // Steps 6.g.i-iii.
            var next = callContentFunction(iterator.next, iterator);
            if (!IsObject(next))
                ThrowTypeError(JSMSG_NEXT_RETURNED_PRIMITIVE);

            // Step 6.g.iv.
            if (next.done) {
                A.length = k;
                return A;
            }

            // Steps 6.g.v-vi.
            var nextValue = next.value;

            // Steps 6.g.vii-viii.
            var mappedValue = mapping ? callContentFunction(mapfn, thisArg, nextValue, k) : nextValue;

            // Steps 6.g.ix-xi.
            _DefineDataProperty(A, k++, mappedValue);
        }
    }

    // Step 7.
    assert(usingIterator === undefined, "`items` can't be an Iterable after step 6.g.iv");

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
        var mappedValue = mapping ? callContentFunction(mapfn, thisArg, kValue, k) : kValue;

        // Steps 16.f-g.
        _DefineDataProperty(A, k, mappedValue);
    }

    // Steps 17-18.
    A.length = len;

    // Step 19.
    return A;
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

// ES 2016 draft Mar 25, 2016 22.1.2.5.
function ArraySpecies() {
    // Step 1.
    return this;
}

// ES 2016 draft Mar 25, 2016 9.4.2.3.
function ArraySpeciesCreate(originalArray, length) {
    // Step 1.
    assert(typeof length == "number", "length should be a number");
    assert(length >= 0, "length should be a non-negative number");

    // Step 2.
    if (length === -0)
        length = 0;

    // Step 4, 6.
    if (!IsArray(originalArray))
        return std_Array(length);

    // Step 5.a.
    var C = originalArray.constructor;

    // Step 5.b.
    if (IsConstructor(C) && IsWrappedArrayConstructor(C))
        return std_Array(length);

    // Step 5.c.
    if (IsObject(C)) {
        // Step 5.c.i.
        C = C[std_species];

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
    if (!IsObject(O))
        return false;

    // Step 2.
    var spreadable = O[std_isConcatSpreadable];

    // Step 3.
    if (spreadable !== undefined)
        return ToBoolean(spreadable);

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
                    _DefineDataProperty(A, n, E[k]);

                    // Step 5.c.iv.4.
                    n++;
                }
            } else {
                // Step 5.c.i, 5.c.iv, and 5.c.iv.5.
                for (k = 0; k < len; k++) {
                    // Steps 5.c.iv.1-3.
                    if (k in E)
                        _DefineDataProperty(A, n, E[k]);

                    // Step 5.c.iv.4.
                    n++;
                }
            }
        } else {
            // Step 5.d.i.
            if (n >= MAX_NUMERIC_INDEX)
                ThrowTypeError(JSMSG_TOO_LONG_ARRAY);

            // Step 5.d.ii.
            _DefineDataProperty(A, n, E);

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

function ArrayStaticConcat(arr, arg1) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.concat');
    var args = callFunction(std_Array_slice, arguments, 1);
    return callFunction(std_Function_apply, ArrayConcat, arr, args);
}

function ArrayStaticJoin(arr, separator) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.join');
    return callFunction(std_Array_join, arr, separator);
}

function ArrayStaticReverse(arr) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.reverse');
    return callFunction(std_Array_reverse, arr);
}

function ArrayStaticSort(arr, comparefn) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.sort');
    return callFunction(std_Array_sort, arr, comparefn);
}

function ArrayStaticPush(arr, arg1) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.push');
    var args = callFunction(std_Array_slice, arguments, 1);
    return callFunction(std_Function_apply, std_Array_push, arr, args);
}

function ArrayStaticPop(arr) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.pop');
    return callFunction(std_Array_pop, arr);
}

function ArrayStaticShift(arr) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.shift');
    return callFunction(std_Array_shift, arr);
}

function ArrayStaticUnshift(arr, arg1) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.unshift');
    var args = callFunction(std_Array_slice, arguments, 1);
    return callFunction(std_Function_apply, std_Array_unshift, arr, args);
}

function ArrayStaticSplice(arr, start, deleteCount) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.splice');
    var args = callFunction(std_Array_slice, arguments, 1);
    return callFunction(std_Function_apply, std_Array_splice, arr, args);
}

function ArrayStaticSlice(arr, start, end) {
    if (arguments.length < 1)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, 'Array.slice');
    return callFunction(std_Array_slice, arr, start, end);
}
