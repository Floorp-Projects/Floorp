/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 562448;
var summary = 'Function.prototype.apply should accept any arraylike arguments';
print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function expectTypeError(fun, msg)
{
  try
  {
    fun();
    assertEq(true, false, "should have thrown a TypeError");
  }
  catch (e)
  {
    assertEq(e instanceof TypeError, true, msg + "; instead threw " + e);
  }
}

function fun() { }

var global = this;


/* Step 1. */
var nonfuns = [null, 1, -1, 2.5, "[[Call]]", undefined, true, false, {}];
for (var i = 0, sz = nonfuns.length; i < sz; i++)
{
  var f = function()
  {
    Function.prototype.apply.apply(nonfuns[i], [1, 2, 3]);
  };
  var msg =
    "expected TypeError calling Function.prototype.apply with uncallable this";
  expectTypeError(f, msg);
}


/* Step 2. */
var thisObj = {};

var currentThis, currentThisBox;
function funLength()
{
  assertEq(arguments.length, 0, "should have been called with no arguments");
  assertEq(this, currentThis, "wrong this");
}
function strictFunLength()
{
  "use strict";
  assertEq(arguments.length, 0, "should have been called with no arguments");
  assertEq(this, currentThis, "wrong this");
}

currentThis = global;
funLength.apply();
funLength.apply(undefined);
funLength.apply(undefined, undefined);
funLength.apply(undefined, null);

currentThis = undefined;
strictFunLength.apply();
strictFunLength.apply(undefined);
strictFunLength.apply(undefined, undefined);
strictFunLength.apply(undefined, null);

currentThis = null;
strictFunLength.apply(null);
strictFunLength.apply(null, undefined);
strictFunLength.apply(null, null);

currentThis = thisObj;
funLength.apply(thisObj);
funLength.apply(thisObj, null);
funLength.apply(thisObj, undefined);
strictFunLength.apply(thisObj);
strictFunLength.apply(thisObj, null);
strictFunLength.apply(thisObj, undefined);

currentThis = 17;
strictFunLength.apply(17);
strictFunLength.apply(17, null);
strictFunLength.apply(17, undefined);

function funThisPrimitive()
{
  assertEq(arguments.length, 0, "should have been called with no arguments");
  assertEq(this instanceof currentThisBox, true,
           "this not instanceof " + currentThisBox);
  assertEq(this.valueOf(), currentThis,
           "wrong this valueOf()");
}

currentThis = 17;
currentThisBox = Number;
funThisPrimitive.apply(17);
funThisPrimitive.apply(17, undefined);
funThisPrimitive.apply(17, null);

currentThis = "foopy";
currentThisBox = String;
funThisPrimitive.apply("foopy");
funThisPrimitive.apply("foopy", undefined);
funThisPrimitive.apply("foopy", null);

currentThis = false;
currentThisBox = Boolean;
funThisPrimitive.apply(false);
funThisPrimitive.apply(false, undefined);
funThisPrimitive.apply(false, null);


/* Step 3. */
var nonobjs = [1, -1, 2.5, "[[Call]]", true, false];
for (var i = 0, sz = nonobjs.length; i < sz; i++)
{
  var f = function() { fun.apply(thisObj, nonobjs[i]); };
  var msg = "should have thrown a TypeError with non-object arguments";
  expectTypeError(f, msg);
}


/* Step 4. */
var args = { get length() { throw 42; } };
try
{
  fun.apply(thisObj, args);
}
catch (e)
{
  assertEq(e, 42, "didn't throw result of [[Get]] on arguments object");
}


/*
 * NB: There was an erratum removing the steps numbered 5 and 7 in the original
 *     version of ES5; see also the comments in js_fun_apply.
 */

/* Step 5. */
var called = false;
var argsObjectLength =
  { length: { valueOf: function() { called = true; return 17; } } };

fun.apply({}, argsObjectLength);
assertEq(called, true, "should have been set in valueOf called via ToUint32");

var upvar = "unset";
var argsObjectPrimitiveLength =
  {
    length:
      {
        valueOf: function() { upvar = "valueOf"; return {}; },
        toString: function()
        {
          upvar = upvar === "valueOf" ? "both" : "toString";
          return 17;
        }
      }
  };
fun.apply({}, argsObjectPrimitiveLength);
assertEq(upvar, "both", "didn't call all hooks properly");


/* Step 6-9. */
var seenThis, res, steps;
var argsAccessors =
  {
    length: 4,
    get 0() { steps.push("0"); return 1; },
    get 1() { steps.push("1"); return 2; },
    // make sure values shine through holes
    get 3() { steps.push("3"); return 8; },
  };

Object.prototype[2] = 729;

seenThis = "not seen";
function argsAsArray()
{
  seenThis = this;
  steps.push(Math.PI);
  return Array.prototype.map.call(arguments, function(v) { return v; });
}

steps = [];
res = argsAsArray.apply(thisObj, argsAccessors);
assertEq(seenThis, thisObj, "saw wrong this");

assertEq(steps.length, 4, "wrong steps: " + steps);
assertEq(steps[0], "0", "bad step 0");
assertEq(steps[1], "1", "bad step 1");
assertEq(steps[2], "3", "bad step 3");
assertEq(steps[3], Math.PI, "bad last step");

assertEq(res.length, 4, "wrong return: " + res);
assertEq(res[0], 1, "wrong ret[0]");
assertEq(res[1], 2, "wrong ret[0]");
assertEq(res[2], 729, "wrong ret[0]");
assertEq(res[3], 8, "wrong ret[0]");

seenThis = "not seen";
function strictArgsAsArray()
{
  "use strict";
  seenThis = this;
  steps.push(NaN);
  return Array.prototype.map.call(arguments, function(v) { return v; });
}

steps = [];
res = strictArgsAsArray.apply(null, argsAccessors);
assertEq(seenThis, null, "saw wrong this");

assertEq(steps.length, 4, "wrong steps: " + steps);
assertEq(steps[0], "0", "bad step 0");
assertEq(steps[1], "1", "bad step 1");
assertEq(steps[2], "3", "bad step 3");
assertEq(steps[3], 0 / 0, "bad last step");

assertEq(res.length, 4, "wrong return: " + res);
assertEq(res[0], 1, "wrong ret[0]");
assertEq(res[1], 2, "wrong ret[0]");
assertEq(res[2], 729, "wrong ret[0]");
assertEq(res[3], 8, "wrong ret[0]");

strictArgsAsArray.apply(17, argsAccessors);
assertEq(seenThis, 17, "saw wrong this");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
