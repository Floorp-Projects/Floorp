/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 657585;
var summary =
  'Guard against infinite recursion when converting |this| to string for the ' +
  'String.prototype.* methods';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

try
{
  var obj = {};
  obj.toString = String.prototype.charAt;
  "" + obj;
  throw new Error("should have thrown");
}
catch (e)
{
  assertEq(e instanceof InternalError, true,
           "should have thrown InternalError for over-recursion, got: " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
