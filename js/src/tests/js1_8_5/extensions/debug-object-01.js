// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

checkFunction(this, "Debug", 1);

assertEq(Debug.prototype.constructor, Debug);
assertEq(Object.prototype.toString.call(Debug.prototype), "[object Debug]");
assertEq(Object.getPrototypeOf(Debug.prototype), Object.prototype);

reportCompare(0, 0, 'ok');
