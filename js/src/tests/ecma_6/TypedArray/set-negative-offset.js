/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gTestfile = "set-negative-offset.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 1140752;
var summary =
  "%TypedArray%.prototype.set must throw a RangeError when passed a negative " +
  "offset";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

try
{
  new Uint8Array().set([], -1);
  throw new Error("didn't throw at all");
}
catch (e)
{
  assertEq(e instanceof RangeError, true,
           "expected RangeError, instead got: " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
