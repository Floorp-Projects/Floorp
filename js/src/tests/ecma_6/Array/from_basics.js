/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Array.from copies arrays.
var src = [1, 2, 3], copy = Array.from(src);
assertEq(copy === src, false);
assertEq(Array.isArray(copy), true);
assertDeepEq(copy, src);

// Non-element properties are not copied.
var a = [0, 1];
a.name = "lisa";
assertDeepEq(Array.from(a), [0, 1]);

// It's a shallow copy.
src = [[0], [1]];
copy = Array.from(src);
assertEq(copy[0], src[0]);
assertEq(copy[1], src[1]);

// Array.from can copy non-iterable objects, if they're array-like.
src = {0: "zero", 1: "one", length: 2};
copy = Array.from(src);
assertEq(Array.isArray(copy), true);
assertDeepEq(copy, ["zero", "one"]);

// Properties past the .length are not copied.
src = {0: "zero", 1: "one", 2: "two", 9: "nine", name: "lisa", length: 2};
assertDeepEq(Array.from(src), ["zero", "one"]);

// If an object has neither an @@iterator method nor .length,
// then it's treated as zero-length.
assertDeepEq(Array.from({}), []);

// Source object property order doesn't matter.
src = {length: 2, 1: "last", 0: "first"};
assertDeepEq(Array.from(src), ["first", "last"]);

// Array.from does not preserve holes.
assertDeepEq(Array.from(Array(3)), [undefined, undefined, undefined]);
assertDeepEq(Array.from([, , 2, 3]), [undefined, undefined, 2, 3]);
assertDeepEq(Array.from([0, , , ,]), [0, undefined, undefined, undefined]);

// Even on non-iterable objects.
assertDeepEq(Array.from({length: 4}), [undefined, undefined, undefined, undefined]);

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
