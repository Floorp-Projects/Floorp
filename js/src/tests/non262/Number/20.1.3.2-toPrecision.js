// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------

var BUGNUMBER = 818617;
var summary = "ECMAScript 2017 Draft ECMA-262 Section 20.1.3.5: Number.prototype.toPrecision";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// With NaN, fractionDigits out of range.
assertEq(Number.prototype.toPrecision.call(NaN, 555), 'NaN');

// With NaN, fractionDigits in range.
assertEq(Number.prototype.toPrecision.call(NaN, 5), 'NaN');

// With Infinity, fractionDigits out of range.
assertEq(Number.prototype.toPrecision.call(Infinity, 555), 'Infinity');

// With Infinity, fractionDigits in range.
assertEq(Number.prototype.toPrecision.call(Infinity, 5), 'Infinity');

// With -Infinity, fractionDigits out of range.
assertEq(Number.prototype.toPrecision.call(-Infinity, 555), '-Infinity');

// With -Infinity, fractionDigits in range.
assertEq(Number.prototype.toPrecision.call(-Infinity, 5), '-Infinity');

// With NaN, function assigning a value.
let x = 10;
assertEq(Number.prototype.toPrecision.call(NaN, { valueOf() { x = 20; return 1; } }), 'NaN');
assertEq(x, 20);

// With NaN, function throwing an exception.
assertThrows(() => Number.prototype.toPrecision.call(NaN, { valueOf() { throw "hello"; } }));

// Not a number throws TypeError
assertThrowsInstanceOf(() => Number.prototype.toPrecision.call("Hello"), TypeError);

if (typeof reportCompare === "function") {
  reportCompare(true, true);
}

