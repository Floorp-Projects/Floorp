// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1183400;
var summary =
  "Deletion of a && or || expression that constant-folds to a name must not " +
  "attempt to delete the name";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

Object.defineProperty(this, "nonconfigurable", { value: 42 });
assertEq(nonconfigurable, 42);

assertEq(delete nonconfigurable, false);
assertEq(delete (true && nonconfigurable), true);

function nested()
{
  assertEq(delete nonconfigurable, false);
  assertEq(delete (true && nonconfigurable), true);
}
nested();

function nestedStrict()
{
  "use strict";
  assertEq(delete (true && nonconfigurable), true);
}
nestedStrict();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
