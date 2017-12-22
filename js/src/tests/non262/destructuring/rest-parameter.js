/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Simple functional test for destructuring rest parameters.

function arrayRest(...[a, b]) {
    return a + b;
}
assertEq(arrayRest(3, 7), 10);


function arrayRestWithDefault(...[a, b = 1]) {
    return a + b;
}
assertEq(arrayRestWithDefault(3, 7), 10);
assertEq(arrayRestWithDefault(4), 5);
assertEq(arrayRestWithDefault(4, undefined), 5);


function objectRest(...{length: len}) {
    return len;
}
assertEq(objectRest(), 0);
assertEq(objectRest(10), 1);
assertEq(objectRest(10, 20), 2);


function objectRestWithDefault(...{0: a, 1: b = 1}) {
    return a + b;
}
assertEq(objectRestWithDefault(3, 7), 10);
assertEq(objectRestWithDefault(4), 5);
assertEq(objectRestWithDefault(4, undefined), 5);


function arrayRestWithNestedRest(...[...r]) {
    return r.length;
}
assertEq(arrayRestWithNestedRest(), 0);
assertEq(arrayRestWithNestedRest(10), 1);
assertEq(arrayRestWithNestedRest(10, 20), 2);


function arrayRestTDZ(...[a = a]) { }
assertThrowsInstanceOf(() => arrayRestTDZ(), ReferenceError);


function objectRestTDZ(...{a = a}) { }
assertThrowsInstanceOf(() => objectRestTDZ(), ReferenceError);


if (typeof reportCompare === "function")
    reportCompare(0, 0);
