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

function testOuterLet()
{
  return eval("let x; (function() { return delete x; })");
}

f = testOuterLet();

assertEq(f(), false); // can't delete lexical declarations => false


function testOuterForLet()
{
  return eval("for (let x; false; ); (function() { return delete x; })");
}

f = testOuterForLet();

assertEq(f(), true); // not there => true (only non-configurable => false)


function testOuterForInLet()
{
  return eval("for (let x in {}); (function() { return delete x; })");
}

f = testOuterForInLet();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // not there => true (only non-configurable => false)


function testOuterNestedVarInForLet()
{
  return eval("for (let q = 0; q < 5; q++) { var x; } (function() { return delete x; })");
}

f = testOuterNestedVarInForLet();

assertEq(f(), true); // configurable, so remove => true
assertEq(f(), true); // there => true


function testArgumentShadowLet()
{
  return eval("let x; (function(x) { return delete x; })");
}

f = testArgumentShadowLet();

assertEq(f(), false); // non-configurable argument => false


function testFunctionLocal()
{
  return eval("(function() { let x; return delete x; })");
}

f = testFunctionLocal();

assertEq(f(), false); // defined by function code => not configurable => false


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
