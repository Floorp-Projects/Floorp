// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
"use strict"

//-----------------------------------------------------------------------------
var BUGNUMBER = 649570;
var summary = "|delete window.NaN| should throw a TypeError";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var g = this, v = false;
try
{
  delete this.NaN;
  throw new Error("no exception thrown");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "Expected a TypeError, got: " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
