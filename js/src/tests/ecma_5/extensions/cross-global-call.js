// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 631135;
var summary =
  "Objects created by/for natives are created for the caller's scope, not " +
  "for the native's scope";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var otherGlobal = newGlobal("same-compartment");

function makeArray()
{
  return new otherGlobal.Array();
}

var a;

a = makeArray();
assertEq(a instanceof otherGlobal.Array, true);
assertEq(a instanceof Array, false);
a = makeArray();
assertEq(a instanceof otherGlobal.Array, true);
assertEq(a instanceof Array, false);
a = makeArray();
assertEq(a instanceof otherGlobal.Array, true);
assertEq(a instanceof Array, false);

for (var i = 0; i < 3; i++)
{
  a = new otherGlobal.Array();
  assertEq(a instanceof otherGlobal.Array, true);
  assertEq(a instanceof Array, false);
}


var res = otherGlobal.Array.prototype.slice.call([1, 2, 3], 0, 1);
assertEq(res instanceof otherGlobal.Array, true);
assertEq(res instanceof Array, false);

var obj = { p: 1 };
var desc = otherGlobal.Object.getOwnPropertyDescriptor(obj, "p");
assertEq(desc instanceof otherGlobal.Object, true);
assertEq(desc instanceof Object, false);

try
{
  otherGlobal.Function.prototype.call.call(null);
  var err = "no error";
}
catch (e)
{
  err = e;
}
assertEq(err instanceof otherGlobal.TypeError, true,
         "bad error: " + err);
assertEq(err instanceof TypeError, false, "bad error: " + err);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
