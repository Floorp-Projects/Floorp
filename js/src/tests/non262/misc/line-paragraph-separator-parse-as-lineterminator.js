/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 663331;
var summary =
  "U+2028 LINE SEPARATOR and U+2029 PARAGRAPH SEPARATOR must match the " +
  "LineTerminator production when parsing code";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var hidden = 17;
var assigned;

assigned = 42;
assertEq(eval('"use strict"; var hidden\u2028assigned = 5; typeof hidden'),
         "undefined");
assertEq(assigned, 5);

assigned = 42;
function t1()
{
  assertEq(eval('var hidden\u2028assigned = 5; typeof hidden'), "undefined");
  assertEq(assigned, 5);
}
t1();

assigned = 42;
assertEq(eval('"use strict"; var hidden\u2029assigned = 5; typeof hidden'),
         "undefined");
assertEq(assigned, 5);

assigned = 42;
function t2()
{
  assertEq(eval('var hidden\u2029assigned = 5; typeof hidden'), "undefined");
  assertEq(assigned, 5);
}
t2();

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
