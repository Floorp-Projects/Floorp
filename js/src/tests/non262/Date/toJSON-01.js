// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'toJSON-01.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 584811;
var summary = "Date.prototype.toJSON isn't to spec";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var called;

var dateToJSON = Date.prototype.toJSON;
assertEq(Date.prototype.hasOwnProperty("toJSON"), true);
assertEq(typeof dateToJSON, "function");

// brief test to exercise this outside of isolation, just for sanity
var invalidDate = new Date();
invalidDate.setTime(NaN);
assertEq(JSON.stringify({ p: invalidDate }), '{"p":null}');


/* 15.9.5.44 Date.prototype.toJSON ( key ) */
assertEq(dateToJSON.length, 1);

/*
 * 1. Let O be the result of calling ToObject, giving it the this value as its
 *    argument.
 */
try
{
  dateToJSON.call(null);
  throw new Error("should have thrown a TypeError");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "ToObject throws TypeError for null/undefined");
}

try
{
  dateToJSON.call(undefined);
  throw new Error("should have thrown a TypeError");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "ToObject throws TypeError for null/undefined");
}


/*
 * 2. Let tv be ToPrimitive(O, hint Number).
 * ...expands to:
 *    1. Let valueOf be the result of calling the [[Get]] internal method of object O with argument "valueOf".
 *    2. If IsCallable(valueOf) is true then,
 *       a. Let val be the result of calling the [[Call]] internal method of valueOf, with O as the this value and
 *                an empty argument list.
 *       b. If val is a primitive value, return val.
 *    3. Let toString be the result of calling the [[Get]] internal method of object O with argument "toString".
 *    4. If IsCallable(toString) is true then,
 *       a. Let str be the result of calling the [[Call]] internal method of toString, with O as the this value and
 *               an empty argument list.
 *       b. If str is a primitive value, return str.
 *    5. Throw a TypeError exception.
 */
try
{
  var r = dateToJSON.call({ get valueOf() { throw 17; } });
  throw new Error("didn't throw, returned: " + r);
}
catch (e)
{
  assertEq(e, 17, "bad exception: " + e);
}

called = false;
assertEq(dateToJSON.call({ valueOf: null,
                           toString: function() { called = true; return 12; },
                           toISOString: function() { return "ohai"; } }),
         "ohai");
assertEq(called, true);

called = false;
assertEq(dateToJSON.call({ valueOf: function() { called = true; return 42; },
                           toISOString: function() { return null; } }),
         null);
assertEq(called, true);

try
{
  called = false;
  dateToJSON.call({ valueOf: function() { called = true; return {}; },
                    get toString() { throw 42; } });
}
catch (e)
{
  assertEq(called, true);
  assertEq(e, 42, "bad exception: " + e);
}

called = false;
assertEq(dateToJSON.call({ valueOf: function() { called = true; return {}; },
                           get toString() { return function() { return 8675309; }; },
                           toISOString: function() { return true; } }),
         true);
assertEq(called, true);

var asserted = false;
called = false;
assertEq(dateToJSON.call({ valueOf: function() { called = true; return {}; },
                           get toString()
                           {
                             assertEq(called, true);
                             asserted = true;
                             return function() { return 8675309; };
                           },
                           toISOString: function() { return NaN; } }),
         NaN);
assertEq(asserted, true);

try
{
  var r = dateToJSON.call({ valueOf: null, toString: null,
                            get toISOString()
                            {
                              throw new Error("shouldn't have been gotten");
                            } });
  throw new Error("didn't throw, returned: " + r);
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "bad exception: " + e);
}


/* 3. If tv is a Number and is not finite, return null. */
assertEq(dateToJSON.call({ valueOf: function() { return Infinity; } }), null);
assertEq(dateToJSON.call({ valueOf: function() { return -Infinity; } }), null);
assertEq(dateToJSON.call({ valueOf: function() { return NaN; } }), null);

assertEq(dateToJSON.call({ valueOf: function() { return Infinity; },
                           toISOString: function() { return {}; } }), null);
assertEq(dateToJSON.call({ valueOf: function() { return -Infinity; },
                           toISOString: function() { return []; } }), null);
assertEq(dateToJSON.call({ valueOf: function() { return NaN; },
                           toISOString: function() { return undefined; } }), null);


/*
 * 4. Let toISO be the result of calling the [[Get]] internal method of O with
 *    argument "toISOString".
 */
try
{
  var r = dateToJSON.call({ get toISOString() { throw 42; } });
  throw new Error("didn't throw, returned: " + r);
}
catch (e)
{
  assertEq(e, 42, "bad exception: " + e);
}


/* 5. If IsCallable(toISO) is false, throw a TypeError exception. */
try
{
  var r = dateToJSON.call({ toISOString: null });
  throw new Error("didn't throw, returned: " + r);
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "bad exception: " + e);
}

try
{
  var r = dateToJSON.call({ toISOString: undefined });
  throw new Error("didn't throw, returned: " + r);
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "bad exception: " + e);
}

try
{
  var r = dateToJSON.call({ toISOString: "oogabooga" });
  throw new Error("didn't throw, returned: " + r);
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "bad exception: " + e);
}

try
{
  var r = dateToJSON.call({ toISOString: Math.PI });
  throw new Error("didn't throw, returned: " + r);
}
catch (e)
{
  assertEq(e instanceof TypeError, true, "bad exception: " + e);
}


/*
 * 6. Return the result of calling the [[Call]] internal method of toISO with O
 *    as the this value and an empty argument list.
 */
var o =
  {
    toISOString: function(a)
    {
      called = true;
      assertEq(this, o);
      assertEq(a, undefined);
      assertEq(arguments.length, 0);
      return obj;
    }
  };
var obj = {};
called = false;
assertEq(dateToJSON.call(o), obj, "should have gotten obj back");
assertEq(called, true);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
