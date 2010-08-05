// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 583925;
var summary =
  "parseInt should treat leading-zero inputs as octal regardless of whether caller is strict or laissez-faire mode code";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(parseInt("08"), 0);
assertEq(parseInt("09"), 0);
assertEq(parseInt("014"), 12);

function strictParseInt(s) { "use strict"; return parseInt(s); }

assertEq(strictParseInt("08"), 0);
assertEq(strictParseInt("09"), 0);
assertEq(strictParseInt("014"), 12);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
