/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 501739;
var summary =
  "String.prototype.match should zero the .lastIndex when called with a " +
  "global RegExp";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var s = '0x2x4x6x8';
var p = /x/g;
p.lastIndex = 3;

var arr = s.match(p);
assertEq(arr.length, 4);
arr.forEach(function(v) { assertEq(v, "x"); });
assertEq(p.lastIndex, 0);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
