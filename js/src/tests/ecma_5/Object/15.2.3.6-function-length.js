// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = '15.2.3.6-function-length.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 430133;
var summary = 'ES5 Object.defineProperty(O, P, Attributes): Function.length';

print(BUGNUMBER + ": " + summary);

load("ecma_5/Object/defineProperty-setup.js");

/**************
 * BEGIN TEST *
 **************/

try
{
  new TestRunner().runFunctionLengthTests();
}
catch (e)
{
  throw "Error thrown during testing: " + e +
          " at line " + e.lineNumber + "\n" +
        (e.stack
          ? "Stack: " + e.stack.split("\n").slice(2).join("\n") + "\n"
          : "");
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete!");
