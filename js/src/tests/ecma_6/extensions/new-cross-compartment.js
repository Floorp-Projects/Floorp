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

// THIS IS WRONG.  |new| itself should throw if !IsConstructor(constructor),
// meaning this global's TypeError should be used.  The problem ultimately is
// that wrappers conflate callable/constructible, so any old function from
// another global appears to be both.  Somebody fix bug XXXXXX!
try
{
  var constructor = g.parseInt;
  new constructor();
  throw new Error("no error thrown");
}
catch (e)
{
  assertEq(e instanceof g.TypeError, true,
           "THIS REALLY SHOULD BE |e instanceof TypeError|");
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
