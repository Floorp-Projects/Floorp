/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Array.from on a string iterates over the string.
assertDeepEq(Array.from("test string"),
             ['t', 'e', 's', 't', ' ', 's', 't', 'r', 'i', 'n', 'g']);

// Array.from on a string handles surrogate pairs correctly.
var gclef = "\uD834\uDD1E"; // U+1D11E MUSICAL SYMBOL G CLEF
assertDeepEq(Array.from(gclef), [gclef]);
assertDeepEq(Array.from(gclef + " G"), [gclef, " ", "G"]);

// Array.from on a string calls the @@iterator method.
String.prototype[Symbol.iterator] = function* () { yield 1; yield 2; };
assertDeepEq(Array.from("anything"), [1, 2]);

// If the iterator method is deleted, Strings are still arraylike.
delete String.prototype[Symbol.iterator];
assertDeepEq(Array.from("works"), ['w', 'o', 'r', 'k', 's']);
assertDeepEq(Array.from(gclef), ['\uD834', '\uDD1E']);

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
