// Test fast-path for String.split("").

load(libdir + 'eqArrayHelper.js');

assertEqArray("".split(""), []);
assertEqArray("a".split(""), ["a"]);
assertEqArray("abc".split(""), ["a", "b", "c"]);

assertEqArray("abcd".split("", 2), ["a", "b"]);
assertEqArray("abcd".split("", 0), []);
assertEqArray("abcd".split("", -1), ["a", "b", "c", "d"]);

// Note: V8 disagrees about this one, but we are correct by ecma-262 15.5.4.14 part 9.
assertEqArray("abcd".split(undefined, 0), []);

assertEqArray("abcd".split(undefined, 1), ["abcd"]);
