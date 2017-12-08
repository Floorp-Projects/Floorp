/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */
"use strict";

const x = Object.freeze([1, 2, 3]);

let fakeArray = {
    [Symbol.isConcatSpreadable]: true,
    length: 2,
    0: "hello",
    1: "world"
}
assertDeepEq(x.concat(fakeArray), [1, 2, 3, "hello", "world"]);
assertDeepEq(x.concat(fakeArray, fakeArray), [1, 2, 3, "hello", "world", "hello", "world"]);

for (let truthy of [true, 3.41, "abc", Symbol(), {}]) {
    let obj = {[Symbol.isConcatSpreadable]: truthy, length: 1, 0: "hey"}
    assertDeepEq(x.concat(obj), [1, 2, 3, "hey"]);
}

for (let notTruthy of [null, undefined, false, 0, NaN, ""]) {
    let obj = {[Symbol.isConcatSpreadable]: notTruthy, length: 1, 0: "hey"}
    assertDeepEq(x.concat(obj), [1, 2, 3, obj]);
}

let array = [5, 4];
assertDeepEq(x.concat(array), [1, 2, 3, 5, 4]);

// Can make arrays non-spreadable
array[Symbol.isConcatSpreadable] = false;
assertDeepEq(x.concat(array), [1, 2, 3, [5, 4]]);

// Explicitly spreadable
array[Symbol.isConcatSpreadable] = true;
assertDeepEq(x.concat(array), [1, 2, 3, 5, 4]);

reportCompare(true, true);
