/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommonn.org/licenses/publicdomain/
 */

var BUGNUMBER = 747197;
var summary = "TimeClip behavior for very large numbers";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function addToLimit(n) { return 8.64e15 + n; }

assertEq(8.64e15 === addToLimit(0.0), true);
assertEq(8.64e15 === addToLimit(0.5), true);
assertEq(8.64e15 === addToLimit(0.5000000000000001), false);

var times =
  [Number.MAX_VALUE,
   -Number.MAX_VALUE,
   Infinity,
   -Infinity,
   addToLimit(0.5000000000000001),
   -addToLimit(0.5000000000000001)];

for (var i = 0, len = times.length; i < len; i++)
{
  var d = new Date();
  assertEq(d.setTime(times[i]), NaN, "times[" + i + "]");
  assertEq(d.getTime(), NaN);
  assertEq(d.valueOf(), NaN);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
