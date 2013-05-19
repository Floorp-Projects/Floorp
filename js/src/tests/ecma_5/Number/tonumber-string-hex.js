/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommonn.org/licenses/publicdomain/
 */

var BUGNUMBER = 872853;
var summary = 'Various tests of ToNumber(string), particularly +"0x" being NaN';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(+"0x", NaN);
assertEq(+"\t0x", NaN);
assertEq(+"0x\n", NaN);
assertEq(+"\n0x\t", NaN);
assertEq(+"0x0", 0);
assertEq(+"0xa", 10);
assertEq(+"0xff", 255);
assertEq(+"-0x", NaN);
assertEq(+"-0xa", NaN);
assertEq(+"-0xff", NaN);
assertEq(+"0xInfinity", NaN);
assertEq(+"+Infinity", Infinity);
assertEq(+"-Infinity", -Infinity);
assertEq(+"\t+Infinity", Infinity);
assertEq(+"-Infinity\n", -Infinity);
assertEq(+"+ Infinity", NaN);
assertEq(+"- Infinity", NaN);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
