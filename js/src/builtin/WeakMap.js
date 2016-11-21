/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES2017 draft rev 0e10c9f29fca1385980c08a7d5e7bb3eb775e2e4
// 23.3.1.1 WeakMap, steps 6-8
function WeakMapConstructorInit(iterable) {
    var map = this;

    // Step 6.a.
    var adder = map.set;

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
        var nextItem = next.value;

        // Step 8.d.
        if (!IsObject(nextItem)) {
            IteratorCloseThrow(iter);
            ThrowTypeError(JSMSG_INVALID_MAP_ITERABLE, "WeakMap");
        }

        // Steps 8.e-j.
        try {
            callContentFunction(adder, map, nextItem[0], nextItem[1]);
        } catch (e) {
            IteratorCloseThrow(iter);
            throw e;
        }
    }
}
