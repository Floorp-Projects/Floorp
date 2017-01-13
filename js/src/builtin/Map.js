/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES2017 draft rev 0e10c9f29fca1385980c08a7d5e7bb3eb775e2e4
// 23.1.1.1 Map, steps 6-8
function MapConstructorInit(iterable) {
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
            ThrowTypeError(JSMSG_INVALID_MAP_ITERABLE, "Map");
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

/* ES6 20121122 draft 15.14.4.4. */
function MapForEach(callbackfn, thisArg = undefined) {
    /* Step 1-2. */
    var M = this;
    if (!IsObject(M))
        ThrowTypeError(JSMSG_INCOMPATIBLE_PROTO, "Map", "forEach", typeof M);

    /* Step 3-4. */
    try {
        callFunction(std_Map_has, M);
    } catch (e) {
        // has will throw on non-Map objects, throw our own error in that case.
        ThrowTypeError(JSMSG_INCOMPATIBLE_PROTO, "Map", "forEach", typeof M);
    }

    /* Step 5. */
    if (!IsCallable(callbackfn))
        ThrowTypeError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 6-8. */
    var entries = callFunction(std_Map_iterator, M);
    while (true) {
        var result = callFunction(MapIteratorNext, entries);
        if (result.done)
            break;
        var entry = result.value;
        callContentFunction(callbackfn, thisArg, entry[1], entry[0], M);
    }
}

function MapEntries() {
    return callFunction(std_Map_iterator, this);
}
_SetCanonicalName(MapEntries, "entries");

var iteratorTemp = { mapIterationResultPair : null };

function MapIteratorNext() {
    // Step 1.
    var O = this;

    // Steps 2-3.
    if (!IsObject(O) || !IsMapIterator(O))
        return callFunction(CallMapIteratorMethodIfWrapped, O, "MapIteratorNext");

    // Steps 4-5 (implemented in _GetNextMapEntryForIterator).
    // Steps 8-9 (omitted).

    var mapIterationResultPair = iteratorTemp.mapIterationResultPair;
    if (!mapIterationResultPair) {
        mapIterationResultPair = iteratorTemp.mapIterationResultPair =
            _CreateMapIterationResultPair();
    }

    var retVal = {value: undefined, done: true};

    // Step 10.a, 11.
    var done = _GetNextMapEntryForIterator(O, mapIterationResultPair);
    if (!done) {
        // Steps 10.b-c (omitted).

        // Step 6.
        var itemKind = UnsafeGetInt32FromReservedSlot(this, ITERATOR_SLOT_ITEM_KIND);

        var result;
        if (itemKind === ITEM_KIND_KEY) {
            // Step 10.d.i.
            result = mapIterationResultPair[0];
        } else if (itemKind === ITEM_KIND_VALUE) {
            // Step 10.d.ii.
            result = mapIterationResultPair[1];
        } else {
            // Step 10.d.iii.
            assert(itemKind === ITEM_KIND_KEY_AND_VALUE, itemKind);
            result = [mapIterationResultPair[0], mapIterationResultPair[1]];
        }

        mapIterationResultPair[0] = null;
        mapIterationResultPair[1] = null;
        retVal.value = result;
        retVal.done = false;
    }

    // Steps 7, 12.
    return retVal;
}

// ES6 final draft 23.1.2.2.
function MapSpecies() {
    // Step 1.
    return this;
}
_SetCanonicalName(MapSpecies, "get [Symbol.species]");
