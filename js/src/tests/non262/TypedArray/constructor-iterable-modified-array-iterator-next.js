/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Construct typed array from array with modified array iterator next method.
const ArrayIteratorPrototype = Object.getPrototypeOf([][Symbol.iterator]());
const origNext = ArrayIteratorPrototype.next;
const modifiedNext = function() {
    let {value, done} = origNext.call(this);
    return {value: value * 5, done};
};
for (let constructor of anyTypedArrayConstructors) {
    for (let array of [[], [1], [2, 3], [4, 5, 6], Array(1024).fill(0).map((v, i) => i % 24)]) {
        ArrayIteratorPrototype.next = modifiedNext;
        let typedArray;
        try {
            typedArray = new constructor(array);
        } finally {
            ArrayIteratorPrototype.next = origNext;
        }

        assertEq(typedArray.length, array.length);
        assertEqArray(typedArray, array.map(v => v * 5));
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
