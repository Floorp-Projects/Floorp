/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TypedArray = Object.getPrototypeOf(Int32Array);

assertEq(TypedArray.length, 0);

assertDeepEq(Object.getOwnPropertyDescriptor(TypedArray, "length"), {
    value: 0, writable: false, enumerable: false, configurable: true,
});

if (typeof reportCompare === "function")
    reportCompare(0, 0);
