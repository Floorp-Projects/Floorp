/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Construct typed array from iterable packed array.
for (let constructor of anyTypedArrayConstructors) {
    for (let array of [[], [1], [2, 3], [4, 5, 6], Array(1024).fill(0).map((v, i) => i % 128)]) {
        let typedArray = new constructor(array);

        assertEq(typedArray.length, array.length);
        assertEqArray(typedArray, array);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
