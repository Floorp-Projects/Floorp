// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 637643;
var summary =
  "new Int8Array([1, 2, 3]).subarray(1).subarray(1)[0] === 3";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var ta = new Int8Array([1, 2, 3]);
assertEq(ta.length, 3);
assertEq(ta[0], 1);
assertEq(ta[1], 2);
assertEq(ta[2], 3);

var sa1 = ta.subarray(1);
assertEq(sa1.length, 2);
assertEq(sa1[0], 2);
assertEq(sa1[1], 3);

var sa2 = sa1.subarray(1);
assertEq(sa2.length, 1);
assertEq(sa2[0], 3);

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
