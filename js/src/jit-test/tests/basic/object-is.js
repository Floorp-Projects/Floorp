/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var list = [
    [1, 1, true],
    [0, 1, false],
    [3.5, 3.5, true],
    [0, 0, true],
    [0, -0, false],
    [-0, 0, false],
    [-0, -0, true],

    [true, true, true],
    [true, false, false],
    [false, false, true],

    [NaN, NaN, true],
    [NaN, undefined, false],
    [Infinity, -Infinity, false],
    [Infinity, Infinity, true],
]

for (var test of list) {
    assertEq(Object.is(test[0], test[1]), test[2])
}

var obj = {}
assertEq(Object.is(obj, obj), true);
assertEq(Object.is(obj, {}), false);
assertEq(Object.is([], []), false);

assertEq(Object.is(null, null, null), true);

/* Not defined parameters are undefined ... */
assertEq(Object.is(null), false);
assertEq(Object.is(undefined), true);
assertEq(Object.is(), true);

assertEq(Object.is.length, 2);
