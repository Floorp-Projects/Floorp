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


// accessor identity

function strictMode() { "use strict"; return 42; }
var canonicalTTE = Object.getOwnPropertyDescriptor(strictMode, "caller").get;

var barCaller = Object.getOwnPropertyDescriptor(bar, "caller");
assertEq("get" in barCaller, true);
assertEq("set" in barCaller, true);
assertEq(barCaller.get, canonicalTTE);
assertEq(barCaller.set, canonicalTTE);

var barArguments = Object.getOwnPropertyDescriptor(bar, "arguments");
assertEq("get" in barArguments, true);
assertEq("set" in barArguments, true);
assertEq(barArguments.get, canonicalTTE);
assertEq(barArguments.set, canonicalTTE);

var bazCaller = Object.getOwnPropertyDescriptor(baz, "caller");
assertEq("get" in bazCaller, true);
assertEq("set" in bazCaller, true);
assertEq(bazCaller.get, canonicalTTE);
assertEq(bazCaller.set, canonicalTTE);

var bazArguments = Object.getOwnPropertyDescriptor(baz, "arguments");
assertEq("get" in bazArguments, true);
assertEq("set" in bazArguments, true);
assertEq(bazArguments.get, canonicalTTE);
assertEq(bazArguments.set, canonicalTTE);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
