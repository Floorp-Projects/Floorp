var BUGNUMBER = 918879;
var summary = 'String.fromCodePoint';

print(BUGNUMBER + ": " + summary);

// Tests taken from:
// https://github.com/mathiasbynens/String.fromCodePoint/blob/master/tests/tests.js

assertEq(String.fromCodePoint.length, 0);
assertEq(String.propertyIsEnumerable('fromCodePoint'), false);

assertEq(String.fromCodePoint(''), '\0');
assertEq(String.fromCodePoint(), '');
assertEq(String.fromCodePoint(-0), '\0');
assertEq(String.fromCodePoint(0), '\0');
assertEq(String.fromCodePoint(0x1D306), '\uD834\uDF06');
assertEq(String.fromCodePoint(0x1D306, 0x61, 0x1D307), '\uD834\uDF06a\uD834\uDF07');
assertEq(String.fromCodePoint(0x61, 0x62, 0x1D307), 'ab\uD834\uDF07');
assertEq(String.fromCodePoint(false), '\0');
assertEq(String.fromCodePoint(null), '\0');

assertThrowsInstanceOf(function() { String.fromCodePoint('_'); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint('+Infinity'); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint('-Infinity'); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(-1); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(0x10FFFF + 1); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(3.14); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(3e-2); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(Infinity); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(NaN); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint(undefined); }, RangeError);
assertThrowsInstanceOf(function() { String.fromCodePoint({}); }, RangeError);

var counter = Math.pow(2, 15) * 3 / 2;
var result = [];
while (--counter >= 0) {
        result.push(0); // one code unit per symbol
}
String.fromCodePoint.apply(null, result); // must not throw

var counter = Math.pow(2, 15) * 3 / 2;
var result = [];
while (--counter >= 0) {
        result.push(0xFFFF + 1); // two code units per symbol
}
String.fromCodePoint.apply(null, result); // must not throw

reportCompare(0, 0, "ok");
