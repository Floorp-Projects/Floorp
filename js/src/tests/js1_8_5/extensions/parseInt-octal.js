// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'parseInt-octal.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 577536;
var summary =
  "parseInt should treat leading-zero inputs as decimal in strict mode";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(parseInt("08"), 0);
assertEq(parseInt("09"), 0);
assertEq(parseInt("014"), 12);

function strictParseInt(s) { "use strict"; return parseInt(s); }

assertEq(strictParseInt("08"), 8);
assertEq(strictParseInt("09"), 9);
assertEq(strictParseInt("014"), 14);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
