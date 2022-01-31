/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function TupleToArray(obj) {
    var len = TupleLength(obj);
    var items = std_Array(len);

    for(var k = 0; k < len; k++) {
        DefineDataProperty(items, k, obj[k]);
    }
    return items;
}

// proposal-record-tuple
// Tuple.prototype.toSorted()
function TupleToSorted(comparefn) {
    /* Step 1. */
    if (comparefn !== undefined && !IsCallable(comparefn)) {
        ThrowTypeError(JSMSG_BAD_SORT_ARG);
    }

    /* Step 2. */
    var T = ThisTupleValue(this);

    /* Step 3. */
    var items = TupleToArray(T);
    var sorted = callFunction(ArraySort, items, comparefn);
    return std_Tuple_unchecked(sorted);
}

// proposal-record-tuple
// Tuple.prototype.toSpliced()
function TupleToSpliced(start, deleteCount, /*, ...items */) {
    /* Steps 1-2. */
    var list = ThisTupleValue(this);

    /* Step 3. */
    var len = TupleLength(list);

    /* Step 4. */
    var relativeStart = ToInteger(start);

    /* Step 5. */
    var actualStart;
    if (relativeStart < 0) {
        actualStart = std_Math_max(len + relativeStart, 0);
    } else {
        actualStart = std_Math_min(relativeStart, len);
    }

    /* Step 6. */
    var insertCount;
    var actualDeleteCount;
    if (arguments.length === 0) {
        insertCount = 0;
        actualDeleteCount = 0;
    } else if (arguments.length === 1) {
        /* Step 7. */
        insertCount = 0;
        actualDeleteCount = len - actualStart;
    } else {
        /* Step 8a. */
        insertCount = arguments.length - 2;
        /* Step 8b. */
        let dc = ToInteger(deleteCount);
        /* Step 8c. */
        actualDeleteCount = std_Math_min(std_Math_max(dc, 0), len - actualStart);
    }

    /* Step 9. */
    if (len + insertCount - actualDeleteCount > MAX_NUMERIC_INDEX) {
        ThrowTypeError(JSMSG_TOO_LONG_ARRAY);
    }


    /* Step 10. */
    var k = 0;
    /* Step 11. */
    /* Step 12. */
    var itemCount = insertCount;

    /* Step 13. */
    var newList = [];

    /* Step 14. */
    while(k < actualStart) {
        /* Step 14a. */
        let E = list[k];
        /* Step 14b. */
        DefineDataProperty(newList, k, E);
        /* Step 14c. */
        k++;
    }

    /* Step 15. */
    var itemK = 0;
    /* Step 16. */
    while(itemK < itemCount) {
        /* Step 16a. */
        let E = arguments[itemK + 2];
        /* Step 16b. */
        if (IsObject(E)) {
            ThrowTypeError(JSMSG_RECORD_TUPLE_NO_OBJECT);
        }
        /* Step 16c. */
        DefineDataProperty(newList, k, E);
        /* Step 16d. */
        k++;
        itemK++;
    }

    /* Step 17. */
    itemK = actualStart + actualDeleteCount;
    /* Step 18. */
    while(itemK < len) {
        /* Step 18a. */
        let E = list[itemK];
        /* Step 18b. */
        DefineDataProperty(newList, k, E);
        /* Step 18c. */
        k++;
        itemK++;
    }

    /* Step 19. */
    return std_Tuple_unchecked(newList);
}

// proposal-record-tuple
// Tuple.prototype.toReversed()
function TupleToReversed() {
    /* Step 1. */
    var T = ThisTupleValue(this);

    /* Step 2 not necessary */

    /* Step 3. */
    var len = TupleLength(T);
    var newList = [];

    /* Step 4. */
    for (var k = len - 1; k >= 0; k--) {
        /* Step 5a. */
        let E = T[k];
        /* Step 5b. */
        DefineDataProperty(newList, (len - k) - 1, E);
    }

    /* Step 5. */
    return std_Tuple_unchecked(newList);
}

// proposal-record-tuple
// Tuple.prototype.concat()
function TupleConcat() {
    /* Step 1 */
    var T = ThisTupleValue(this);
    /* Step 2 (changed to include all elements from T). */
    var list = TupleToArray(T);
    /* Step 3 */
    var n = list.length;
    /* Step 4 not necessary due to changed step 2. */
    /* Step 5 */
    for(var i = 0; i < arguments.length; i++) {
        /* Step 5a. */
        let E = arguments[i];
        /* Step 5b. */
        var spreadable = IsConcatSpreadable(E);
        /* Step 5c. */
        if (spreadable) {
            /* Step 5c.i. */
            var k = 0;
            /* Step 5c.ii */
            var len = ToLength(E.length);
            /* Step 5c.iii */
            if (n + len > MAX_NUMERIC_INDEX) {
                ThrowTypeError(JSMSG_TOO_LONG_ARRAY);
            }
            /* Step 5c.iv */
            while(k < len) {
                /* Step 5c.iv.2 */
                var exists = E[k] !== undefined;
                /* Step 5c.iv.3 */
                if (exists) {
                    /* Step 5c.iv.3.a */
                    var subElement = E[k];
                    /* Step 5c.iv.3.b */
                    if (IsObject(subElement)) {
                        ThrowTypeError(JSMSG_RECORD_TUPLE_NO_OBJECT);
                    }
                    /* Step 5c.iv.3.c */
                    DefineDataProperty(list, n, subElement);
                    /* Step 5c.iv.4 */
                    n++;
                }
                /* Step 5c.iv.5 */
                k++;
            }
        }
        /* Step 5d. */
        else {
            /* Step 5d.ii. */
            if (n >= MAX_NUMERIC_INDEX) {
                ThrowTypeError(JSMSG_TOO_LONG_ARRAY);
            }
            /* Step 5d.iii. */
            if (IsObject(E)) {
                ThrowTypeError(JSMSG_RECORD_TUPLE_NO_OBJECT);
            }
            /* Step 5d.iv. */
            DefineDataProperty(list, n, E);
            /* Step 5d.v. */
            n++;
        }
    }
    /* Step 6 */
    return std_Tuple_unchecked(list);
}

// proposal-record-tuple
// Tuple.prototype.includes()
function TupleIncludes(valueToFind /* , fromIndex */) {
    var fromIndex = arguments.length > 1 ? arguments[1] : undefined;
    return callContentFunction(ArrayIncludes, ThisTupleValue(this),
                                   valueToFind, fromIndex);
}

// proposal-record-tuple
// Tuple.prototype.indexOf()
function TupleIndexOf(valueToFind /* , fromIndex */) {
    var fromIndex = arguments.length > 1 ? arguments[1] : undefined;
    return callContentFunction(ArrayIndexOf, ThisTupleValue(this),
                               valueToFind, fromIndex);
}

// proposal-record-tuple
// Tuple.prototype.join()
function TupleJoin(separator) {
    let T = ThisTupleValue(this);

    // Step 2
    let len = TupleLength(T);

    // Steps 3-4
    var sep = ",";
    if (separator != undefined && separator !== null) {
        let toString = IsCallable(separator.toString) ? separator.toString : std_Object_toString;
        sep = callContentFunction(toString, separator);
    }

    // Step 5
    var R = "";

    // Step 6
    var k = 0;

    // Step 7
    while (k < len) {
        // Step 7a
        if (k > 0) {
            R += sep;
        }
        // Step 7b
        let element = T[k];
        // Step 7c
        var next = "";
        if (element != undefined && element != null) {
            let toString = IsCallable(element.toString) ? element.toString : std_Object_toString;
            next = callContentFunction(toString, element);
        }
        // Step 7d
        R += next;
        // Step 7e
        k++;
    }
    // Step 8
    return R;
}


// proposal-record-tuple
// Tuple.prototype.lastIndexOf()
function TupleLastIndexOf(valueToFind /* , fromIndex */) {
    if (arguments.length < 2) {
        return callContentFunction(ArrayLastIndexOf, ThisTupleValue(this),
                                   valueToFind);
    }
    return callContentFunction(ArrayLastIndexOf, ThisTupleValue(this),
                                   valueToFind, arguments[1]);
}

// proposal-record-tuple
// Tuple.prototype.toString()
function TupleToString() {
    /* Step 1. */
    var T = ThisTupleValue(this);
    /* Step 2. */
    var func = T.join;
    if (!IsCallable(func)) {
        return callFunction(std_Object_toString, T);
    }
    /* Step 4. */
    return callContentFunction(func, T);
}

// proposal-record-tuple
// Tuple.prototype.toLocaleString()
function TupleToLocaleString(locales, options) {
    var T = ThisTupleValue(this);
    return callContentFunction(ArrayToLocaleString, TupleToArray(T),
                               locales, options);
}

// proposal-record-tuple
// Tuple.prototype.entries()
function TupleEntries() {
    return CreateArrayIterator(this, ITEM_KIND_KEY_AND_VALUE);
}

// proposal-record-tuple
// Tuple.prototype.keys()
function TupleKeys() {
    return CreateArrayIterator(this, ITEM_KIND_KEY);
}

// proposal-record-tuple
// Tuple.prototype.values()
function $TupleValues() {
    return CreateArrayIterator(this, ITEM_KIND_VALUE);
};

SetCanonicalName($TupleValues, "values");

// proposal-record-tuple
// Tuple.prototype.every()
function TupleEvery(callbackfn) {
    return callContentFunction(ArrayEvery, ThisTupleValue(this),
                               callbackfn);
}

// proposal-record-tuple
// Tuple.prototype.filter()
function TupleFilter(callbackfn) {

    /* Steps 1-2 */
    var list = ThisTupleValue(this);

    /* Step 3. */
    var len = TupleLength(list);

    /* Step 4. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Tuple.prototype.filter");
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 5. */
    var newList = [];

    /* Step 6. */
    var k = 0;
    var newK = 0;

    /* Step 7. */
    var T = arguments.length > 1 ? arguments[1] : void 0;
    while(k < len) {
        /* Step 7a. */
        var kValue = list[k];
        /* Step 7b. */
        var selected = ToBoolean(callContentFunction(callbackfn, T, kValue, k, list));
        /* Step 7c. */
        if (selected) {
            /* Step 7c.i. */
            DefineDataProperty(newList, newK++, kValue);
        }
        /* Step 7d. */
        k++;
    }

    /* Step 8. */
    return std_Tuple_unchecked(newList);
}

// proposal-record-tuple
// Tuple.prototype.find()
function TupleFind(predicate) {
    return callContentFunction(ArrayFind, ThisTupleValue(this),
                               predicate);
}

// proposal-record-tuple
// Tuple.prototype.findIndex()
function TupleFindIndex(predicate) {
    return callContentFunction(ArrayFindIndex, ThisTupleValue(this),
                               predicate);
}

// proposal-record-tuple
// Tuple.prototype.forEach()
function TupleForEach(callbackfn) {
    return callContentFunction(ArrayForEach, ThisTupleValue(this),
                               callbackfn);
}

// proposal-record-tuple
// Tuple.prototype.map()
function TupleMap(callbackfn) {

    /* Steps 1-2. */
    var list = ThisTupleValue(this);

    /* Step 3. */
    var len = TupleLength(list);

    /* Step 4. */
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 5. */
    var newList = [];

    /* Steps 6-7. */
    var thisArg = arguments.length > 1 ? arguments[1] : void 0;
    for(var k = 0; k < len; k++) {
        /* Step 7a. */
        var kValue = list[k];
        /* Step 7b. */
        var mappedValue = callContentFunction(callbackfn, thisArg, kValue, k, list);
        /* Step 7c */
        if (IsObject(mappedValue)) {
            ThrowTypeError(JSMSG_RECORD_TUPLE_NO_OBJECT);
        }
        /* Step 7d. */
        DefineDataProperty(newList, k, mappedValue);
    }

    /* Step 8. */
    return std_Tuple_unchecked(newList);
}

// proposal-record-tuple
// Tuple.prototype.reduce()
function TupleReduce(callbackfn /*, initialVal */) {
    if (arguments.length < 2) {
        return callContentFunction(ArrayReduce, ThisTupleValue(this),
                                   callbackfn);
    }
    return callContentFunction(ArrayReduce, ThisTupleValue(this),
                                   callbackfn, arguments[1]);
}

// proposal-record-tuple
// Tuple.prototype.reduceRight()
function TupleReduceRight(callbackfn /*, initialVal*/) {
    if (arguments.length < 2) {
        return callContentFunction(ArrayReduceRight, ThisTupleValue(this),
                                   callbackfn);
    }
    return callContentFunction(ArrayReduceRight, ThisTupleValue(this),
                                   callbackfn, arguments[1]);
}

// proposal-record-tuple
// Tuple.prototype.some()
function TupleSome(callbackfn) {
    return callContentFunction(ArraySome, ThisTupleValue(this),
                               callbackfn);
}

function FlattenIntoTuple(target, source, depth) {
    /* Step 1. */
    assert(IsArray(target), "FlattenIntoTuple: target is not array");

    /* Step 2. */
    assert(IsTuple(source), "FlattenIntoTuple: source is not tuple");

    /* Step 3 not needed. */

    /* Step 4. */
    var mapperFunction = undefined;
    var thisArg = undefined;
    var mapperIsPresent = arguments.length > 3;
    if (mapperIsPresent) {
        mapperFunction = arguments[3];
        assert(IsCallable(mapperFunction) && arguments.length > 4 && depth === 1,
               "FlattenIntoTuple: mapper function must be callable, thisArg present, and depth === 1");
        thisArg = arguments[4];
    }

    /* Step 5. */
    var sourceIndex = 0;

    /* Step 6. */
    for (var k = 0; k < source.length; k++) {
        var element = source[k];
        /* Step 6a. */
        if (mapperIsPresent) {
            /* Step 6a.i. */
            element = callContentFunction(mapperFunction, thisArg, element, sourceIndex, source);
            /* Step 6a.ii. */
            if (IsObject(element)) {
                ThrowTypeError(JSMSG_RECORD_TUPLE_NO_OBJECT);
            }
        }
        /* Step 6b. */
        if (depth > 0 && IsTuple(element)) {
            FlattenIntoTuple(target, element, depth - 1);
        } else {
            /* Step 6c.i. */
            var len = ToLength(target.length);
            /* Step 6c.ii. */
            if (len > MAX_NUMERIC_INDEX) {
                ThrowTypeError(JSMSG_TOO_LONG_ARRAY);
            }
            /* Step 6c.iii. */
            DefineDataProperty(target, len, element);
        }
        /* Step 6d. */
        sourceIndex++;
    }
}

// proposal-record-tuple
// Tuple.prototype.flat()
function TupleFlat() {
    /* Steps 1-2. */
    var list = ThisTupleValue(this);

    /* Step 3. */
    var depthNum = 1;

    /* Step 4. */
    if (arguments.length > 0 && arguments[0] !== undefined) {
        depthNum = ToInteger(arguments[0]);
    }

    /* Step 5. */
    var flat = [];

    /* Step 6. */
    FlattenIntoTuple(flat, list, depthNum);

    /* Step 7. */
    return std_Tuple_unchecked(flat);
}

// proposal-record-tuple
// Tuple.prototype.flatMap()
function TupleFlatMap(mapperFunction /*, thisArg*/) {
    /* Steps 1-2. */
    var list = ThisTupleValue(this);

    /* Step 3. */
    if (arguments.length === 0)
        ThrowTypeError(JSMSG_MISSING_FUN_ARG, 0, "Tuple.prototype.flatMap");
    if (!IsCallable(mapperFunction))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, mapperFunction));

    /* Step 4. */
    var flat = [];

    /* Step 5. */
    var thisArg = arguments.length > 1 ? arguments[1] : void 0;
    FlattenIntoTuple(flat, list, 1, mapperFunction, thisArg);

    /* Step 6. */
    return std_Tuple_unchecked(flat);
}
