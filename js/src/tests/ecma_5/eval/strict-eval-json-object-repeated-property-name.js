// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
"use strict";

//-----------------------------------------------------------------------------
var BUGNUMBER = 657713;
var summary =
  "eval JSON parser hack misfires in strict mode, for objects with duplicate " +
  "property names";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

try
{
  var r = eval('({"a": 2, "a": 3})');
  throw new Error("didn't throw, returned " + r);
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true,
           "strict mode forbids objects with duplicated property names: " + e);
}

try
{
  var r = Function("'use strict'; return eval('({\"a\": 2, \"a\": 3})');")();
  throw new Error("didn't throw, returned " + r);
}
catch (e)
{
  assertEq(e instanceof SyntaxError, true,
           "strict mode forbids objects with duplicated property names: " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete!");
