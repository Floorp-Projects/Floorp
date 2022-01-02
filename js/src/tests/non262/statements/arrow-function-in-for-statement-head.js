// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var gTestfile = "arrow-function-in-for-statement-head.js";
var BUGNUMBER = 1163851;
var summary =
  "|for (x => 0 in 1;;) break;| must be a syntax error per ES6, not an " +
  "elaborate nop";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

try
{
  Function("for (x => 0 in 1;;) break;");
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true,
           "expected syntax error, got " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
