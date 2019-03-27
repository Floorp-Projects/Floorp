// |reftest| skip-if(!xulRuntime.shell) -- preventExtensions on global
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 621432;
var summary =
  "If a var statement can't create a global property because the global " +
  "object isn't extensible, and an error is thrown while decompiling the " +
  "global, don't assert";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var toSource = [];
Object.preventExtensions(this);

try
{
  eval("var x;");
  throw new Error("no error thrown");
}
catch (e)
{
  reportCompare(e instanceof TypeError, true, "expected TypeError, got: " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
