/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Construct typed array from object with undefined or null [Symbol.iterator] property.
for (let constructor of anyTypedArrayConstructors) {
    for (let iterator of [undefined, null]) {
        let arrayLike = {
            [Symbol.iterator]: iterator,
            length: 2,
            0: 10,
            1: 20,
        };
        let typedArray = new constructor(arrayLike);

        assertEq(typedArray.length, arrayLike.length);
        assertEqArray(typedArray, arrayLike);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
