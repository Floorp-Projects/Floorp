// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1178653;
var summary =
  "|new| on a cross-compartment wrapper to a non-constructor shouldn't assert";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var g = newGlobal();

var otherStr = new g.String("foo");
assertEq(otherStr instanceof g.String, true);
assertEq(otherStr.valueOf(), "foo");

try
{
  var constructor = g.parseInt;
  new constructor();
  throw new Error("no error thrown");
}
catch (e)
{
  // NOTE: not |g.TypeError|, because |new| itself throws because
  //       |!IsConstructor(constructor)|.
  assertEq(e instanceof TypeError, true);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
