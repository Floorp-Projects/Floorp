/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Construct typed array from object with non-callable [Symbol.iterator] property.
for (let constructor of anyTypedArrayConstructors) {
    for (let iterator of [true, 0, Math.PI, "", "10", Symbol.iterator, {}, []]) {
        assertThrowsInstanceOf(() => new constructor({
            [Symbol.iterator]: iterator
        }), TypeError);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
