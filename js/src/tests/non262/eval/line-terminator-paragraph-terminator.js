// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 657367;
var summary =
  "eval via the JSON parser should parse strings containing U+2028/U+2029 " +
  "(as of <https://tc39.github.io/proposal-json-superset/>, that is)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(eval('("\u2028")'), "\u2028");
assertEq(eval('("\u2029")'), "\u2029");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete!");
