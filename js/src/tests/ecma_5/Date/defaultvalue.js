/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommonn.org/licenses/publicdomain/
 */

var BUGNUMBER = 645464;
var summary =
  "[[DefaultValue]] behavior wrong for Date with overridden valueOf/toString";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function allTests()
{
  var DS = new Date(2010, 1, 1).toString();

  // equality

  var d = new Date(2010, 1, 1);
  assertEq(d == DS, true);

  var d2 = new Date(2010, 1, 1);
  d2.valueOf = function() { assertEq(arguments.length, 0); return 17; };
  assertEq(d2 == DS, true);

  var d3 = new Date(2010, 1, 1);
  d3.toString = function() { return 42; };
  assertEq(d3 == 42, true);

  function testEquality()
  {
    var d = new Date(2010, 1, 1);
    assertEq(d == DS, true);

    var d2 = new Date(2010, 1, 1);
    d2.valueOf = function() { assertEq(arguments.length, 0); return 17; };
    assertEq(d2 == DS, true);

    var d3 = new Date(2010, 1, 1);
    d3.toString = function() { return 42; };
    assertEq(d3 == 42, true);
  }
  testEquality();


  // addition of Date to number

  var d = new Date(2010, 1, 1);
  assertEq(d + 5, DS + "5");

  var d2 = new Date(2010, 1, 1);
  d2.toString = function() { return 9; };
  assertEq(d2 + 3, 9 + 3);

  var d3 = new Date(2010, 1, 1);
  d3.valueOf = function() { assertEq(arguments.length, 0); return 17; };
  assertEq(d3 + 5, DS + "5");

  function testDateNumberAddition()
  {
    var d = new Date(2010, 1, 1);
    assertEq(d + 5, DS + "5");

    var d2 = new Date(2010, 1, 1);
    d2.toString = function() { return 9; };
    assertEq(d2 + 3, 9 + 3);

    var d3 = new Date(2010, 1, 1);
    d3.valueOf = function() { assertEq(arguments.length, 0); return 17; };
    assertEq(d3 + 5, DS + "5");
  }
  testDateNumberAddition();


  // addition of Date to Date

  var d = new Date(2010, 1, 1);
  assertEq(d + d, DS + DS);

  var d2 = new Date(2010, 1, 1);
  d2.toString = function() { return 5; };
  assertEq(d2 + d2, 10);

  var d3 = new Date(2010, 1, 1);
  d3.valueOf = function() { assertEq(arguments.length, 0); return 8.5; };
  assertEq(d3 + d3, DS + DS);

  function testDateDateAddition()
  {
    var d = new Date(2010, 1, 1);
    assertEq(d + d, DS + DS);

    var d2 = new Date(2010, 1, 1);
    d2.toString = function() { return 5; };
    assertEq(d2 + d2, 10);

    var d3 = new Date(2010, 1, 1);
    d3.valueOf = function() { assertEq(arguments.length, 0); return 8.5; };
    assertEq(d3 + d3, DS + DS);
  }
  testDateDateAddition();


  // Date as bracketed property name

  var obj = { 8: 42, 9: 73 };
  obj[DS] = 17;

  var d = new Date(2010, 1, 1);
  assertEq(obj[d], 17);

  var d2 = new Date(2010, 1, 1);
  d2.valueOf = function() { assertEq(arguments.length, 0); return 8; }
  assertEq(obj[d2], 17);

  var d3 = new Date(2010, 1, 1);
  d3.toString = function() { return 9; };
  assertEq(obj[d3], 73);

  function testPropertyName()
  {
    var obj = { 8: 42, 9: 73 };
    obj[DS] = 17;

    var d = new Date(2010, 1, 1);
    assertEq(obj[d], 17);

    var d2 = new Date(2010, 1, 1);
    d2.valueOf = function() { assertEq(arguments.length, 0); return 8; }
    assertEq(obj[d2], 17);

    var d3 = new Date(2010, 1, 1);
    d3.toString = function() { return 9; };
    assertEq(obj[d3], 73);
  }
  testPropertyName();


  // Date as property name with |in| operator

  var obj = {};
  obj[DS] = 5;

  var d = new Date(2010, 1, 1);
  assertEq(d in obj, true);

  var d2 = new Date(2010, 1, 1);
  d2.toString = function() { return "baz"; };
  assertEq(d2 in { baz: 42 }, true);

  var d3 = new Date(2010, 1, 1);
  d3.valueOf = function() { assertEq(arguments.length, 0); return "quux"; };
  assertEq(d3 in obj, true);

  function testInOperatorName()
  {
    var obj = {};
    obj[DS] = 5;

    var d = new Date(2010, 1, 1);
    assertEq(d in obj, true);

    var d2 = new Date(2010, 1, 1);
    d2.toString = function() { return "baz"; };
    assertEq(d2 in { baz: 42 }, true);

    var d3 = new Date(2010, 1, 1);
    d3.valueOf = function() { assertEq(arguments.length, 0); return "quux"; };
    assertEq(d3 in obj, true);
  }
  testInOperatorName();
}

allTests();

if (typeof newGlobal === "function")
{
  Date = newGlobal("new-compartment").Date;
  allTests();
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
