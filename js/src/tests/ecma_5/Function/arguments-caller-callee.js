/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'arguments-caller-callee.js';
var BUGNUMBER = 514563;
var summary = "arguments.caller and arguments.callee are poison pills in ES5";

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

function bar() { "use strict"; return arguments; }
expectTypeError(function barCaller() { bar().caller; });
expectTypeError(function barCallee() { bar().callee; });

function baz() { return arguments; }
assertEq(baz().callee, baz);


// accessor identity

function strictMode() { "use strict"; return arguments; }
var canonicalTTE = Object.getOwnPropertyDescriptor(strictMode(), "caller").get;

var args = strictMode();

var argsCaller = Object.getOwnPropertyDescriptor(args, "caller");
assertEq("get" in argsCaller, true);
assertEq("set" in argsCaller, true);
assertEq(argsCaller.get, canonicalTTE);
assertEq(argsCaller.set, canonicalTTE);

var argsCallee = Object.getOwnPropertyDescriptor(args, "callee");
assertEq("get" in argsCallee, true);
assertEq("set" in argsCallee, true);
assertEq(argsCallee.get, canonicalTTE);
assertEq(argsCallee.set, canonicalTTE);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
