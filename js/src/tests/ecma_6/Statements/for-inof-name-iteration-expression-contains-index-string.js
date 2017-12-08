// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var gTestfile = "for-inof-name-iteration-expression-contains-index-string.js";
var BUGNUMBER = 1235640;
var summary =
  "Don't assert parsing a for-in/of loop whose target is a name, where the " +
  "expression being iterated over contains a string containing an index";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function f()
{
  var x;
  for (x in "9")
    continue;
  assertEq(x, "0");
}

f();

function g()
{
  "use strict";
  var x = "unset";
  for (x in arguments)
    continue;
  assertEq(x, "unset");
}

g();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
