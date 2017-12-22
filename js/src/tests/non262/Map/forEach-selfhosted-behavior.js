/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 987243;
var summary = "Don't use .call(...) in the self-hosted Map.prototype.forEach";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var functionCall = Function.prototype.call;

function throwSyntaxError()
{
  throw new SyntaxError("Function.prototype.call incorrectly called");
}

function lalala() {}

Function.prototype.call = throwSyntaxError;

new Map().forEach(throwSyntaxError);
new Map([[1, 2]]).forEach(lalala);
new Map([[1, 2], [3, 4]]).forEach(lalala);

Function.prototype.call = function() { this.set(42, "fnord"); };

new Map().forEach(throwSyntaxError);
new Map([[1, 2]]).forEach(lalala);
new Map([[1, 2], [3, 4]]).forEach(lalala);

var callCount = 0;
Function.prototype.call = function() { callCount++; };

new Map().forEach(throwSyntaxError);
new Map([[1, 2]]).forEach(lalala);
new Map([[1, 2], [3, 4]]).forEach(lalala);

assertEq(callCount, 0);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
