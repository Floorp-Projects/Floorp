// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 588251;
var summary =
  "fun.caller should throw if that value corresponds to a strict mode " +
  "function";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function nonstrict() { return nonstrict.caller; }
function strict() { "use strict"; return nonstrict(); }
try
{
  strict();
  throw 17;
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "expected TypeError accessing strict mode caller, got: " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
