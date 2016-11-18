/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES2017 draft rev 0e10c9f29fca1385980c08a7d5e7bb3eb775e2e4
// 23.4.1.1 WeakSet, steps 6-8
function WeakSetConstructorInit(iterable) {
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
        try {
            callContentFunction(adder, set, nextValue);
        } catch (e) {
            IteratorCloseThrow(iter);
            throw e;
        }
    }
}

// 23.4.3.1
function WeakSet_add(value) {
    // Steps 1-3.
    var S = this;
    if (!IsObject(S) || !IsWeakSet(S))
        return callFunction(CallWeakSetMethodIfWrapped, this, value, "WeakSet_add");

    // Step 4.,6.
    let entries = UnsafeGetReservedSlot(this, WEAKSET_MAP_SLOT);
    if (!entries)
        ThrowTypeError(JSMSG_INCOMPATIBLE_PROTO, "WeakSet", "add", typeof S);

    // Step 5.
    if (!IsObject(value))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, DecompileArg(0, value));

    // Steps 7-8.
    callFunction(std_WeakMap_set, entries, value, true);

    // Step 8.
    return S;
}

// 23.4.3.3
function WeakSet_delete(value) {
    // Steps 1-3.
    var S = this;
    if (!IsObject(S) || !IsWeakSet(S))
        return callFunction(CallWeakSetMethodIfWrapped, this, value, "WeakSet_delete");

    // Step 4.,6.
    let entries = UnsafeGetReservedSlot(this, WEAKSET_MAP_SLOT);
    if (!entries)
        ThrowTypeError(JSMSG_INCOMPATIBLE_PROTO, "WeakSet", "delete", typeof S);

    // Step 5.
    if (!IsObject(value))
        return false;

    // Steps 7-8.
    return callFunction(std_WeakMap_delete, entries, value);
}

// 23.4.3.4
function WeakSet_has(value) {
    // Steps 1-3.
    var S = this;
    if (!IsObject(S) || !IsWeakSet(S))
        return callFunction(CallWeakSetMethodIfWrapped, this, value, "WeakSet_has");

    // Step 4-5.
    let entries = UnsafeGetReservedSlot(this, WEAKSET_MAP_SLOT);
    if (!entries)
        ThrowTypeError(JSMSG_INCOMPATIBLE_PROTO, "WeakSet", "has", typeof S);

    // Step 6.
    if (!IsObject(value))
        return false;

    // Steps 7-8.
    return callFunction(std_WeakMap_has, entries, value);
}
