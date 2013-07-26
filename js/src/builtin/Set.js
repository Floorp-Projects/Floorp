/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* ES6 20121122 draft 15.16.4.6. */

function SetForEach(callbackfn, thisArg = undefined) {
    /* Step 1-2. */
    var S = this;
    if (!IsObject(S))
        ThrowError(JSMSG_BAD_TYPE, typeof S);

    /* Step 3-4. */
    try {
        std_Set_has.call(S);
    } catch (e) {
        ThrowError(JSMSG_BAD_TYPE, typeof S);
    }

    /* Step 5-6. */
    if (!IsCallable(callbackfn))
        ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, callbackfn));

    /* Step 7-8. */
    var values = std_Set_iterator.call(S);
    while (true) {
        try {
            var entry = std_Set_iterator_next.call(values);
        } catch (err) {
            if (err instanceof StopIteration)
                break;
            throw err;
        }
        callFunction(callbackfn, thisArg, entry, entry, S);
    }
}
