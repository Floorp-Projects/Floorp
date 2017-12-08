var BUGNUMBER = 918879;
var summary = 'String.fromCodePoint';

print(BUGNUMBER + ": " + summary);

// Tests taken from:
// https://github.com/mathiasbynens/String.fromCodePoint/blob/master/tests/tests.js

assertEq(String.fromCodePoint.length, 1);
assertEq(String.fromCodePoint.name, 'fromCodePoint');
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

// str_fromCodePoint_one_arg (single argument, creates an inline string)
assertEq(String.fromCodePoint(0x31), '1');
// str_fromCodePoint_few_args (few arguments, creates an inline string)
// JSFatInlineString::MAX_LENGTH_TWO_BYTE / 2 = floor(11 / 2) = 5
assertEq(String.fromCodePoint(0x31, 0x32), '12');
assertEq(String.fromCodePoint(0x31, 0x32, 0x33), '123');
assertEq(String.fromCodePoint(0x31, 0x32, 0x33, 0x34), '1234');
assertEq(String.fromCodePoint(0x31, 0x32, 0x33, 0x34, 0x35), '12345');
// str_fromCodePoint (many arguments, creates a malloc string)
assertEq(String.fromCodePoint(0x31, 0x32, 0x33, 0x34, 0x35, 0x36), '123456');

reportCompare(0, 0, "ok");
