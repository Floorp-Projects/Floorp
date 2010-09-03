/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 537863;
var summary =
  'undefined, Infinity, and NaN global properties should not be writable';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var desc, old, error;
var global = this;

var names = ["NaN", "Infinity", "undefined"];

for (var i = 0; i < names.length; i++)
{
  var name = names[i];
  desc = Object.getOwnPropertyDescriptor(global, name);
  assertEq(desc !== undefined, true, name + " should be present");
  assertEq(desc.enumerable, false, name + " should not be enumerable");
  assertEq(desc.configurable, false, name + " should not be configurable");
  assertEq(desc.writable, false, name + " should not be writable");

  old = global[name];
  global[name] = 17;
  assertEq(global[name], old, name + " changed on setting?");

  error = "before";
  try
  {
    throw new TypeError("SpiderMonkey doesn't currently implement " +
                        "strict-mode throwing when setting a readonly " +
                        "property, not running this bit of test for now; " +
                        "see bug 537873");

    (function() { "use strict"; global[name] = 42; error = "didn't throw"; })();
  }
  catch (e)
  {
    if (e instanceof TypeError)
      error = "typeerror";
    else
      error = "bad exception: " + e;
  }
  assertEq(error, "typeerror", "wrong strict mode error setting " + name);
}

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
