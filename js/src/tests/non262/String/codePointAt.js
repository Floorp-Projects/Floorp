var BUGNUMBER = 918879;
var summary = 'String.prototype.codePointAt';

print(BUGNUMBER + ": " + summary);

// Tests taken from:
// https://github.com/mathiasbynens/String.prototype.codePointAt/blob/master/tests/tests.js
assertEq(String.prototype.codePointAt.length, 1);
assertEq(String.prototype.propertyIsEnumerable('codePointAt'), false);

// String that starts with a BMP symbol
assertEq('abc\uD834\uDF06def'.codePointAt(''), 0x61);
assertEq('abc\uD834\uDF06def'.codePointAt('_'), 0x61);
assertEq('abc\uD834\uDF06def'.codePointAt(), 0x61);
assertEq('abc\uD834\uDF06def'.codePointAt(-Infinity), undefined);
assertEq('abc\uD834\uDF06def'.codePointAt(-1), undefined);
assertEq('abc\uD834\uDF06def'.codePointAt(-0), 0x61);
assertEq('abc\uD834\uDF06def'.codePointAt(0), 0x61);
assertEq('abc\uD834\uDF06def'.codePointAt(3), 0x1D306);
assertEq('abc\uD834\uDF06def'.codePointAt(4), 0xDF06);
assertEq('abc\uD834\uDF06def'.codePointAt(5), 0x64);
assertEq('abc\uD834\uDF06def'.codePointAt(42), undefined);
assertEq('abc\uD834\uDF06def'.codePointAt(Infinity), undefined);
assertEq('abc\uD834\uDF06def'.codePointAt(Infinity), undefined);
assertEq('abc\uD834\uDF06def'.codePointAt(NaN), 0x61);
assertEq('abc\uD834\uDF06def'.codePointAt(false), 0x61);
assertEq('abc\uD834\uDF06def'.codePointAt(null), 0x61);
assertEq('abc\uD834\uDF06def'.codePointAt(undefined), 0x61);

// String that starts with an astral symbol
assertEq('\uD834\uDF06def'.codePointAt(''), 0x1D306);
assertEq('\uD834\uDF06def'.codePointAt('1'), 0xDF06);
assertEq('\uD834\uDF06def'.codePointAt('_'), 0x1D306);
assertEq('\uD834\uDF06def'.codePointAt(), 0x1D306);
assertEq('\uD834\uDF06def'.codePointAt(-1), undefined);
assertEq('\uD834\uDF06def'.codePointAt(-0), 0x1D306);
assertEq('\uD834\uDF06def'.codePointAt(0), 0x1D306);
assertEq('\uD834\uDF06def'.codePointAt(1), 0xDF06);
assertEq('\uD834\uDF06def'.codePointAt(42), undefined);
assertEq('\uD834\uDF06def'.codePointAt(false), 0x1D306);
assertEq('\uD834\uDF06def'.codePointAt(null), 0x1D306);
assertEq('\uD834\uDF06def'.codePointAt(undefined), 0x1D306);

// Lone high surrogates
assertEq('\uD834abc'.codePointAt(''), 0xD834);
assertEq('\uD834abc'.codePointAt('_'), 0xD834);
assertEq('\uD834abc'.codePointAt(), 0xD834);
assertEq('\uD834abc'.codePointAt(-1), undefined);
assertEq('\uD834abc'.codePointAt(-0), 0xD834);
assertEq('\uD834abc'.codePointAt(0), 0xD834);
assertEq('\uD834abc'.codePointAt(false), 0xD834);
assertEq('\uD834abc'.codePointAt(NaN), 0xD834);
assertEq('\uD834abc'.codePointAt(null), 0xD834);
assertEq('\uD834abc'.codePointAt(undefined), 0xD834);

// Lone low surrogates
assertEq('\uDF06abc'.codePointAt(''), 0xDF06);
assertEq('\uDF06abc'.codePointAt('_'), 0xDF06);
assertEq('\uDF06abc'.codePointAt(), 0xDF06);
assertEq('\uDF06abc'.codePointAt(-1), undefined);
assertEq('\uDF06abc'.codePointAt(-0), 0xDF06);
assertEq('\uDF06abc'.codePointAt(0), 0xDF06);
assertEq('\uDF06abc'.codePointAt(false), 0xDF06);
assertEq('\uDF06abc'.codePointAt(NaN), 0xDF06);
assertEq('\uDF06abc'.codePointAt(null), 0xDF06);
assertEq('\uDF06abc'.codePointAt(undefined), 0xDF06);

(function() { String.prototype.codePointAt.call(undefined); }, TypeError);
assertThrowsInstanceOf(function() { String.prototype.codePointAt.call(undefined, 4); }, TypeError);
assertThrowsInstanceOf(function() { String.prototype.codePointAt.call(null); }, TypeError);
assertThrowsInstanceOf(function() { String.prototype.codePointAt.call(null, 4); }, TypeError);
assertEq(String.prototype.codePointAt.call(42, 0), 0x34);
assertEq(String.prototype.codePointAt.call(42, 1), 0x32);
assertEq(String.prototype.codePointAt.call({ 'toString': function() { return 'abc'; } }, 2), 0x63);

assertThrowsInstanceOf(function() { String.prototype.codePointAt.apply(undefined); }, TypeError);
assertThrowsInstanceOf(function() { String.prototype.codePointAt.apply(undefined, [4]); }, TypeError);
assertThrowsInstanceOf(function() { String.prototype.codePointAt.apply(null); }, TypeError);
assertThrowsInstanceOf(function() { String.prototype.codePointAt.apply(null, [4]); }, TypeError);
assertEq(String.prototype.codePointAt.apply(42, [0]), 0x34);
assertEq(String.prototype.codePointAt.apply(42, [1]), 0x32);
assertEq(String.prototype.codePointAt.apply({ 'toString': function() { return 'abc'; } }, [2]), 0x63);

reportCompare(0, 0, "ok");
