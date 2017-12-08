/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

var gTestfile = '15.2.3.4-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 492849;
var summary = 'ES5: Implement Object.preventExtensions, Object.isExtensible';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(typeof Object.isExtensible, "function");
assertEq(Object.isExtensible.length, 1);

var slowArray = [1, 2, 3];
slowArray.slow = 5;
var objs =
  [{}, { 1: 2 }, { a: 3 }, [], [1], [, 1], slowArray, function a(){}, /a/];

for (var i = 0, sz = objs.length; i < sz; i++)
{
  var o = objs[i];
  assertEq(Object.isExtensible(o), true, "object " + i + " not extensible?");

  var o2 = Object.preventExtensions(o);
  assertEq(o, o2);

  assertEq(Object.isExtensible(o), false, "object " + i + " is extensible?");
}

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
