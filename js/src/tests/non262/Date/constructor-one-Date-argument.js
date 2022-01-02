/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommonn.org/licenses/publicdomain/
 */

var BUGNUMBER = 1187233;
var summary =
  "Passing a Date object to |new Date()| should copy it, not convert it to " +
  "a primitive and create it from that.";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

Date.prototype.toString = Date.prototype.valueOf = null;
var d = new Date(new Date(8675309));
assertEq(d.getTime(), 8675309);

Date.prototype.valueOf = () => 42;
d = new Date(new Date(8675309));
assertEq(d.getTime(), 8675309);

var D = newGlobal().Date;

D.prototype.toString = D.prototype.valueOf = null;
var d = new Date(new D(3141592654));
assertEq(d.getTime(), 3141592654);

D.prototype.valueOf = () => 525600;
d = new Date(new D(3141592654));
assertEq(d.getTime(), 3141592654);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
