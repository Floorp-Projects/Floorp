// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 604504;
var summary = "eval called from a native function is indirect";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var originalEval = eval;

var global = this;
var directCheckCode = "this === global";

function testArrayGeneric()
{
  var global = "psych!";
  var eval = Array.map;

  var mapped = eval([directCheckCode], originalEval);
  assertEq(mapped[0], true);
}

function testStringGeneric()
{
  var global = "psych!";
  var eval = String.replace;

  var newString = eval(directCheckCode, directCheckCode, originalEval);
  assertEq(newString, "true");
}
testStringGeneric();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
