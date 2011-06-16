/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 645464;
var summary =
  "[[DefaultValue]] behavior wrong for String with overridden valueOf/toString";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// equality

var s = new String("c");
assertEq(s == "c", true);

var s2 = new String();
s2.valueOf = function() { return "foo"; };
assertEq(s2 == "foo", true);

var s3 = new String();
s3.toString = function() { return "bar"; };
assertEq(s3 == "", true);

function testEquality()
{
  var s = new String("c");
  assertEq(s == "c", true);

  var s2 = new String();
  s2.valueOf = function() { return "foo"; };
  assertEq(s2 == "foo", true);

  var s3 = new String();
  s3.toString = function() { return "bar"; };
  assertEq(s3 == "", true);
}
testEquality();


// addition of String to string

var s = new String();
assertEq(s + "", "");

var s2 = new String();
s2.toString = function() { return "bar"; };
assertEq(s2 + "", "");

var s3 = new String();
s3.valueOf = function() { return "baz"; };
assertEq(s3 + "", "baz");

function testStringAddition()
{
  var s = new String();
  assertEq(s + "", "");

  var s2 = new String();
  s2.toString = function() { return "bar"; };
  assertEq(s2 + "", "");

  var s3 = new String();
  s3.valueOf = function() { return "baz"; };
  assertEq(s3 + "", "baz");
}
testStringAddition();


// addition of String to String

var s = new String();
assertEq(s + s, "");

var s2 = new String();
s2.toString = function() { return "baz"; };
assertEq(s2 + s2, "");

var s3 = new String();
s3.valueOf = function() { return "quux"; };
assertEq(s3 + s3, "quuxquux");

function testNonStringAddition()
{
  var s = new String();
  assertEq(s + s, "");

  var s2 = new String();
  s2.toString = function() { return "baz"; };
  assertEq(s2 + s2, "");

  var s3 = new String();
  s3.valueOf = function() { return "quux"; };
  assertEq(s3 + s3, "quuxquux");
}
testNonStringAddition();


// String as bracketed property name

var obj = { primitive: 17, valueOf: 42, toString: 8675309 };

var s = new String("primitive");
assertEq(obj[s], 17);

var s2 = new String("primitive");
s2.valueOf = function() { return "valueOf"; }
assertEq(obj[s2], 17);

var s3 = new String("primitive");
s3.toString = function() { return "toString"; };
assertEq(obj[s3], 8675309);

function testPropertyNameToString()
{
  var obj = { primitive: 17, valueOf: 42, toString: 8675309 };

  var s = new String("primitive");
  assertEq(obj[s], 17);

  var s2 = new String("primitive");
  s2.valueOf = function() { return "valueOf"; }
  assertEq(obj[s2], 17);

  var s3 = new String("primitive");
  s3.toString = function() { return "toString"; };
  assertEq(obj[s3], 8675309);
}
testPropertyNameToString();


// String as property name with |in| operator

var s = new String();
assertEq(s in { "": 5 }, true);

var s2 = new String();
s.toString = function() { return "baz"; };
assertEq(s in { baz: 42 }, true);

var s3 = new String();
s3.valueOf = function() { return "quux"; };
assertEq(s3 in { "": 17 }, true);

function testInOperatorName()
{
  var s = new String();
  assertEq(s in { "": 5 }, true);

  var s2 = new String();
  s.toString = function() { return "baz"; };
  assertEq(s in { baz: 42 }, true);

  var s3 = new String();
  s3.valueOf = function() { return "quux"; };
  assertEq(s3 in { "": 17 }, true);
}
testInOperatorName();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
