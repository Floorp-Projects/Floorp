/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors:
 *   Gary Kwong
 *   Jeff Walden
 *   Jason Orendorff
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 713944;
var summary =
  "Don't assert anything about a shape from the property cache until it's " +
  "known the cache entry matches";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var accDesc = { set: function() {} };
var dataDesc = { value: 3 };

function f()
{
  propertyIsEnumerable = {};
}
function g()
{
  propertyIsEnumerable = {};
}

Object.defineProperty(Object.prototype, "propertyIsEnumerable", accDesc);
f();
Object.defineProperty(Object.prototype, "propertyIsEnumerable", dataDesc);
assertEq(propertyIsEnumerable, 3);
f();
assertEq(propertyIsEnumerable, 3);
g();
assertEq(propertyIsEnumerable, 3);



var a = { p1: 1, p2: 2 };
var b = Object.create(a);
Object.defineProperty(a, "p1", {set: function () {}});
for (var i = 0; i < 2; i++)
{
  b.p1 = {};
  Object.defineProperty(a, "p1", {value: 3});
}
assertEq(b.p1, 3);
assertEq(a.p1, 3);

reportCompare(true, true);
