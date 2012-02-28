// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 730810;
var summary = "Don't assert on compound assignment to a const";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

const x = 3;
x += 2;
assertEq(x, 3);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete!");
