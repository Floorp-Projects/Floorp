/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Construct typed array from array with modified array iterator.
const origIterator = Array.prototype[Symbol.iterator];
const modifiedIterator = function*() {
    for (let v of origIterator.call(this)) {
        yield v * 5;
    }
};
for (let constructor of anyTypedArrayConstructors) {
    for (let array of [[], [1], [2, 3], [4, 5, 6], Array(1024).fill(0).map((v, i) => i % 24)]) {
        Array.prototype[Symbol.iterator] = modifiedIterator;
        let typedArray;
        try {
            typedArray = new constructor(array)
        } finally {
            Array.prototype[Symbol.iterator] = origIterator;
        }

        assertEq(typedArray.length, array.length);
        assertEqArray(typedArray, array.map(v => v * 5));
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
