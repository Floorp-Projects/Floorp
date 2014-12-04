/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES6 draft rev28 (2014/10/14) 22.2.3.10 %TypedArray%.prototype.find(predicate [,thisArg]).
function TypedArrayFind(predicate, thisArg = undefined) {
    // This function is not generic.
    if (!IsObject(this) || !IsTypedArray(this)) {
        return callFunction(CallTypedArrayMethodIfWrapped, this, predicate, thisArg,
                            "TypedArrayFind");
    }

    // Steps 1-2.
    var O = this;

    // Steps 3-5.
    var len = TypedArrayLength(O);

    // Step 6.
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, "%TypedArray%.prototype.find");
    if (!IsCallable(predicate))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, predicate));

    // Step 7.
    var T = thisArg;

    // Steps 8-9. */
    // Steps a (implicit), and g.
    for (var k = 0; k < len; k++) {
        // Steps a-c.
        var kValue = O[k];
        // Steps d-f.
        if (callFunction(predicate, T, kValue, k, O))
            return kValue;
    }

    // Step 10.
    return undefined;
}

// ES6 draft rev28 (2014/10/14) 22.2.3.11 %TypedArray%.prototype.findIndex(predicate [,thisArg]).
function TypedArrayFindIndex(predicate, thisArg = undefined) {
    // This function is not generic.
    if (!IsObject(this) || !IsTypedArray(this)) {
        return callFunction(CallTypedArrayMethodIfWrapped, this, predicate, thisArg,
                            "TypedArrayFindIndex");
    }

    // Steps 1-2.
    var O = this;

    // Steps 3-5.
    var len = TypedArrayLength(O);

    // Step 6.
    if (arguments.length === 0)
        ThrowError(JSMSG_MISSING_FUN_ARG, 0, "%TypedArray%.prototype.findIndex");
    if (!IsCallable(predicate))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, predicate));

    // Step 7.
    var T = thisArg;

    // Steps 8-9.
    // Steps a (implicit), and g.
    for (var k = 0; k < len; k++) {
        // Steps a-f.
        if (callFunction(predicate, T, O[k], k, O))
            return k;
    }

    // Step 10.
    return -1;
}
