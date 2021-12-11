/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 635389;
var summary = 'Infinite recursion via [].{toString,toLocaleString,join}';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

try
{
  var x = [];
  x.join = Array.prototype.toString;
  "" + x;
  throw new Error("should have thrown");
}
catch (e)
{
  assertEq(e instanceof InternalError, true,
           "should have thrown for over-recursion");
}

try
{
  var x = { toString: Array.prototype.toString, join: Array.prototype.toString };
  "" + x;
  throw new Error("should have thrown");
}
catch (e)
{
  assertEq(e instanceof InternalError, true,
           "should have thrown for over-recursion");
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
