// |reftest| skip-if(!xulRuntime.shell||!this.hasOwnProperty('Intl'))
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 843004;
var summary =
  "Use of an object that emulates |undefined| as the sole option must " +
  "preclude imputing default values";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var opt = createIsHTMLDDA();
opt.toString = function() { return "long"; };

var str = new Date(2013, 12 - 1, 14).toLocaleString("en-US", { weekday: opt });

// Because "weekday" was present and not undefined (stringifying to "long"),
// this must be a string like "Saturday" (in this implementation, that is).
assertEq(str, "Saturday");

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
