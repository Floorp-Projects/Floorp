// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 616294;
var summary =
  "|delete x| inside a function in eval code, where that eval code includes " +
  "|var x| at top level, actually does delete the binding for x";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var f;

function testOuterVar()
{
  return eval("var x; (function() { return delete x; })");
}

f = testOuterVar();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testOuterFunction()
{
  return eval("function x() { } (function() { return delete x; })");
}

f = testOuterFunction();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testOuterForVar()
{
  return eval("for (var x; false; ); (function() { return delete x; })");
}

f = testOuterForVar();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testOuterForInVar()
{
  return eval("for (var x in {}); (function() { return delete x; })");
}

f = testOuterForInVar();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testOuterNestedVar()
{
  return eval("for (var q = 0; q < 5; q++) { var x; } (function() { return delete x; })");
}

f = testOuterNestedVar();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testOuterNestedConditionalVar()
{
  return eval("for (var q = 0; q < 5; q++) { if (false) { var x; } } (function() { return delete x; })");
}

f = testOuterNestedConditionalVar();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testVarInWith()
{
  return eval("with ({}) { var x; } (function() { return delete x; })");
}

f = testVarInWith();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testForVarInWith()
{
  return eval("with ({}) { for (var x = 0; x < 5; x++); } (function() { return delete x; })");
}

f = testForVarInWith();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testForInVarInWith()
{
  return eval("with ({}) { for (var x in {}); } (function() { return delete x; })");
}

f = testForInVarInWith();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testUnknown()
{
  return eval("nameToDelete = 17; (function() { return delete nameToDelete; })");
}

f = testUnknown();

assertEq(f(), true); // configurable global property, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testArgumentShadow()
{
  return eval("var x; (function(x) { return delete x; })");
}

f = testArgumentShadow();

assertEq(f(), false); // non-configurable argument => false


function testArgument()
{
  return eval("(function(x) { return delete x; })");
}

f = testArgument();

assertEq(f(), false); // non-configurable argument => false


function testFunctionLocal()
{
  return eval("(function() { var x; return delete x; })");
}

f = testFunctionLocal();

assertEq(f(), false); // defined by function code => not configurable => false


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
