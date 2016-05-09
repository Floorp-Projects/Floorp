// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 894653;
var summary =
  "Error.prototype.toString called on function objects should work as on any " +
  "object";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function ErrorToString(v)
{
  return Error.prototype.toString.call(v);
}

// The name property of function objects isn't standardized, so this must be an
// extension-land test.

assertEq(ErrorToString(function f(){}), "f");
assertEq(ErrorToString(function g(){}), "g");
assertEq(ErrorToString(function(){}), "");

var fn1 = function() {};
fn1.message = "ohai";
assertEq(ErrorToString(fn1), "fn1: ohai");

var fn2 = function blerch() {};
fn2.message = "fnord";
assertEq(ErrorToString(fn2), "blerch: fnord");

var fn3 = function() {};
fn3.message = "";
assertEq(ErrorToString(fn3), "fn3");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete!");
