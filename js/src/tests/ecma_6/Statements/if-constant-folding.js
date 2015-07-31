// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var gTestfile = "if-constant-folding.js";
var BUGNUMBER = 1183400;
var summary =
  "Don't crash constant-folding an |if| governed by a truthy constant, whose " +
  "alternative statement is another |if|";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// Perform |if| constant folding correctly when the condition is constantly
// truthy and the alternative statement is another |if|.
if (true)
{
  assertEq(true, true, "sanity");
}
else if (42)
{
  assertEq(false, true, "not reached");
  assertEq(true, false, "also not reached");
}


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
