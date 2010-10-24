/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

var gTestfile = 'object-toString-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 575522;
var summary = '({}).toString.call(null) == "[object Null]", ' +
              '({}).toString.call(undefined) == "[object Undefined]", ';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var toString = Object.prototype.toString;

assertEq(toString.call(null), "[object Null]");
assertEq(toString.call(undefined), "[object Undefined]");

assertEq(toString.call(true), "[object Boolean]");
assertEq(toString.call(false), "[object Boolean]");

assertEq(toString.call(0), "[object Number]");
assertEq(toString.call(-0), "[object Number]");
assertEq(toString.call(1), "[object Number]");
assertEq(toString.call(-1), "[object Number]");
assertEq(toString.call(NaN), "[object Number]");
assertEq(toString.call(Infinity), "[object Number]");
assertEq(toString.call(-Infinity), "[object Number]");

assertEq(toString.call("foopy"), "[object String]");

assertEq(toString.call({}), "[object Object]");


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
