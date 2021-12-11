// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 583925;
var summary =
  "parseInt should treat leading-zero inputs (with radix unspecified) as " +
  "decimal, not octal (this changed in bug 786135)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(parseInt("08"), 8);
assertEq(parseInt("09"), 9);
assertEq(parseInt("014"), 14);

function strictParseInt(s) { "use strict"; return parseInt(s); }

assertEq(strictParseInt("08"), 8);
assertEq(strictParseInt("09"), 9);
assertEq(strictParseInt("014"), 14);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
