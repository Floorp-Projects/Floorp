// |reftest| skip-if(!xulRuntime.shell)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 843004;
var summary =
  "Use of an object that emulates |undefined| as the sole option must " +
  "preclude imputing default values";

print(BUGNUMBER + ": " + summary);

if (typeof Intl !== 'object' && typeof quit == 'function') {
  print("Test skipped");
  reportCompare(true, true);
  quit(0);
}

/**************
 * BEGIN TEST *
 **************/

var opt = objectEmulatingUndefined();
opt.toString = function() { return "long"; };

var str = new Date(2013, 12 - 1, 14).toLocaleString("en-US", { weekday: opt });

// Because "weekday" was present and not undefined (stringifying to "long"),
// this must be a string like "Saturday" (in this implementation, that is).
assertEq(str, "Saturday");

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
