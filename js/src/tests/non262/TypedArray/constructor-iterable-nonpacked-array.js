/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Construct typed array from iterable non-packed array.
for (let constructor of anyTypedArrayConstructors) {
    for (let array of [[,], [,,], Array(1024)]) {
        let typedArray = new constructor(array);

        assertEq(typedArray.length, array.length);
        let expectedArray = Array(array.length).fill(isFloatConstructor(constructor) ? NaN : 0);
        assertEqArray(typedArray, expectedArray);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
