// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var gTestfile = "for-in-with-gc-during-iterator-init.js";
var BUGNUMBER = 1464472;
var summary =
  "Properly trace NativeIterator when a GC occurs during its initialization";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

gczeal(17, 1);
for (var i = 0; i < 100; ++i)
{
  Object.prototype[1012] = "value";
  imports = {};
  for (dmod in imports)
    continue; // gc occurs here converting 1012 to string
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
