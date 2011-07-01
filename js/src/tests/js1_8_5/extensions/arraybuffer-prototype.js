// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 665961;
var summary =
  "ArrayBuffer cannot access properties defined on the prototype chain.";
print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var x = { prop: "value" };
var a = new ArrayBuffer([]);
a.__proto__ = x;
assertEq(a.prop, "value");

ArrayBuffer.prototype.prop = "on prototype";
var b = new ArrayBuffer([]);
assertEq(b.prop, "on prototype");

var c = new ArrayBuffer([]);
assertEq(c.prop, "on prototype");
c.prop = "direct";
assertEq(c.prop, "direct");

assertEq(c.nonexistent, undefined);

reportCompare(true, true);
