/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES2017 draft rev 0e10c9f29fca1385980c08a7d5e7bb3eb775e2e4
// 23.2.1.1 Set, steps 6-8
function SetConstructorInit(iterable) {
    var set = this;

    // Step 6.a.
    var adder = set.add;

    // Step 6.b.
    if (!IsCallable(adder))
        ThrowTypeError(JSMSG_NOT_FUNCTION, typeof adder);

    // Step 6.c.
    var iterFn = iterable[std_iterator];
    if (!IsCallable(iterFn))
        ThrowTypeError(JSMSG_NOT_ITERABLE, DecompileArg(0, iterable));

    var iter = callContentFunction(iterFn, iterable);
    if (!IsObject(iter))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, typeof iter);

    // Step 7 (not applicable).

    // Step 8.
    while (true) {
        // Step 8.a.
        var next = callContentFunction(iter.next, iter);
        if (!IsObject(next))
            ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, typeof next);

        // Step 8.b.
        if (next.done)
            return;

        // Step 8.c.
        var nextValue = next.value;

        // Steps 8.d-e.
        callContentFunction(adder, set, nextValue);
    }
}

/* ES6 20121122 draft 15.16.4.6. */
function SetForEach(callbackfn, thisArg = undefined) {
    /* Step 1-2. */
    var S = this;
    if (!IsObject(S))
        ThrowTypeError(JSMSG_INCOMPATIBLE_PROTO, "Set", "forEach", typeof S);

    /* Step 3-4. */
    try {
        callFunction(std_Set_has, S);
    } catch (e) {
        // has will throw on non-Set objects, throw our own error in that case.
        ThrowTypeError(JSMSG_INCOMPATIBLE_PROTO, "Set", "forEach", typeof S);
    }

    /* Step 5-6. */
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 7-8. */
    var values = callFunction(std_Set_iterator, S);
    while (true) {
        var result = callFunction(std_Set_iterator_next, values);
        if (result.done)
            break;
        var value = result.value;
        callContentFunction(callbackfn, thisArg, value, value, S);
    }
}

// ES6 final draft 23.2.2.2.
function SetSpecies() {
    // Step 1.
    return this;
}
_SetCanonicalName(SetSpecies, "get [Symbol.species]");
