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

ArrayBuffer.prototype.prop = "on prototype";
var b = new ArrayBuffer([]);
assertEq(b.prop, "on prototype");

var c = new ArrayBuffer([]);
assertEq(c.prop, "on prototype");
c.prop = "direct";
assertEq(c.prop, "direct");

assertEq(ArrayBuffer.prototype.prop, "on prototype");
assertEq(new ArrayBuffer([]).prop, "on prototype");

assertEq(c.nonexistent, undefined);

reportCompare(true, true);
