/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

assertEq(Number.prototype.toFixed.call(-Infinity), "-Infinity");
assertEq(Number.prototype.toFixed.call(Infinity), "Infinity");
assertEq(Number.prototype.toFixed.call(NaN), "NaN");

assertThrowsInstanceOf(() => Number.prototype.toFixed.call(-Infinity, 555), RangeError);
assertThrowsInstanceOf(() => Number.prototype.toFixed.call(Infinity, 555), RangeError);
assertThrowsInstanceOf(() => Number.prototype.toFixed.call(NaN, 555), RangeError);

assertThrowsInstanceOf(() => Number.prototype.toFixed.call("Hello"), TypeError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
