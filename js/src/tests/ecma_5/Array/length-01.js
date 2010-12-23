/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 600392;
var summary =
  'Object.preventExtensions([]).length = 0 should do nothing, not throw';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/


function testEmpty()
{
  var a = [];
  assertEq(a.length, 0);
  assertEq(Object.preventExtensions(a), a);
  assertEq(a.length, 0);
  a.length = 0;
  assertEq(a.length, 0);
}
testEmpty();

function testEmptyStrict()
{
  "use strict";
  var a = [];
  assertEq(a.length, 0);
  assertEq(Object.preventExtensions(a), a);
  assertEq(a.length, 0);
  a.length = 0;
  assertEq(a.length, 0);
}
testEmptyStrict();

function testNonEmpty()
{
  var a = [1, 2, 3];
  assertEq(a.length, 3);
  assertEq(Object.preventExtensions(a), a);
  assertEq(a.length, 3);
  a.length = 0;
  assertEq(a.length, 0);
}
testNonEmpty();

function testNonEmptyStrict()
{
  "use strict";
  var a = [1, 2, 3];
  assertEq(a.length, 3);
  assertEq(Object.preventExtensions(a), a);
  assertEq(a.length, 3);
  a.length = 0;
  assertEq(a.length, 0);
}
testNonEmptyStrict();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
