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

function funLength()
{
  assertEq(arguments.length, 0, "should have been called with no arguments");
}

funLength.apply();

function funThisLength()
{
  assertEq(this, thisObj, "should have gotten thisObj as this");
  assertEq(arguments.length, 0, "should have been called with no arguments");
}

funThisLength.apply(thisObj);
funThisLength.apply(thisObj, null);
funThisLength.apply(thisObj, undefined);


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
var steps = [];
var argsAccessors =
  {
    length: 3,
    get 0() { steps.push("0"); return 1; },
    get 1() { steps.push("1"); return 2; },
    get 2() { steps.push("2"); return 4; },
  };

var seenThis = "not seen";
function argsAsArray()
{
  seenThis = this;
  steps.push("3");
  return Array.prototype.map.call(arguments, function(v) { return v; });
}

var res = argsAsArray.apply(thisObj, argsAccessors);
assertEq(seenThis, thisObj, "saw wrong this");

assertEq(steps.length, 4, "wrong steps: " + steps);
for (var i = 0; i < 3; i++)
  assertEq(steps[i], "" + i, "bad step " + i);
assertEq(steps[3], "3", "bad call");

assertEq(res.length, 3, "wrong return: " + res);
for (var i = 0; i < 3; i++)
  assertEq(res[i], 1 << i, "wrong ret[" + i + "]: " + res[i]);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
