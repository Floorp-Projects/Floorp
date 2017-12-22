/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 653789;
var summary = 'Check for too-deep stack when calling toLocaleString';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

try
{
  "" + { toString: Object.prototype.toLocaleString };
  throw new Error("should have thrown on over-recursion");
}
catch (e)
{
  assertEq(e instanceof InternalError, true);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
