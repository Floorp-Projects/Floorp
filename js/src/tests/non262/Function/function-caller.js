/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'function-caller.js';
var BUGNUMBER = 514581;
var summary = "Function.prototype.caller should throw a TypeError for " +
              "strict-mode functions";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// behavior

function expectTypeError(fun)
{
  try
  {
    fun();
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assertEq(e instanceof TypeError, true,
             "expected TypeError calling function" +
             ("name" in fun ? " " + fun.name : "") + ", instead got: " + e);
  }
}

function bar() { "use strict"; }
expectTypeError(function barCaller() { bar.caller; });

function baz() { "use strict"; return 17; }
expectTypeError(function bazCaller() { baz.caller; });

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
