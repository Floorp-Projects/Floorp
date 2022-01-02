/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'propertyIsEnumerable.js';
var BUGNUMBER = 619283;
var summary = "Object.prototype.propertyIsEnumerable";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function expectThrowError(errorCtor, fun)
{
  try
  {
    var r = fun();
    throw "didn't throw TypeError, returned " + r;
  }
  catch (e)
  {
    assertEq(e instanceof errorCtor, true,
             "didn't throw " + errorCtor.prototype.name + ", got: " + e);
  }
}

function expectThrowTypeError(fun)
{
  expectThrowError(TypeError, fun);
}

function withToString(fun)
{
  return { toString: fun };
}

function withValueOf(fun)
{
  return { toString: null, valueOf: fun };
}

var propertyIsEnumerable = Object.prototype.propertyIsEnumerable;

/*
 * 1. Let P be ToString(V).
 */
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable(withToString(function() { fahslkjdfhlkjdsl; }));
});
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable.call(null, withToString(function() { fahslkjdfhlkjdsl; }));
});
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable.call(undefined, withToString(function() { fahslkjdfhlkjdsl; }));
});

expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable(withValueOf(function() { fahslkjdfhlkjdsl; }));
});
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable.call(null, withValueOf(function() { fahslkjdfhlkjdsl; }));
});
expectThrowError(ReferenceError, function()
{
  propertyIsEnumerable.call(undefined, withValueOf(function() { fahslkjdfhlkjdsl; }));
});

expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable(withToString(function() { eval("}"); }));
});
expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable.call(null, withToString(function() { eval("}"); }));
});
expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable.call(undefined, withToString(function() { eval("}"); }));
});

expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable(withValueOf(function() { eval("}"); }));
});
expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable.call(null, withValueOf(function() { eval("}"); }));
});
expectThrowError(SyntaxError, function()
{
  propertyIsEnumerable.call(undefined, withValueOf(function() { eval("}"); }));
});

expectThrowError(RangeError, function()
{
  propertyIsEnumerable(withToString(function() { [].length = -1; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(null, withToString(function() { [].length = -1; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(undefined, withToString(function() { [].length = -1; }));
});

expectThrowError(RangeError, function()
{
  propertyIsEnumerable(withValueOf(function() { [].length = -1; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(null, withValueOf(function() { [].length = -1; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(undefined, withValueOf(function() { [].length = -1; }));
});

expectThrowError(RangeError, function()
{
  propertyIsEnumerable(withToString(function() { [].length = 0.7; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(null, withToString(function() { [].length = 0.7; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(undefined, withToString(function() { [].length = 0.7; }));
});

expectThrowError(RangeError, function()
{
  propertyIsEnumerable(withValueOf(function() { [].length = 0.7; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(null, withValueOf(function() { [].length = 0.7; }));
});
expectThrowError(RangeError, function()
{
  propertyIsEnumerable.call(undefined, withValueOf(function() { [].length = 0.7; }));
});

/*
 * 2. Let O be the result of calling ToObject passing the this value as the
 *    argument.
 */
expectThrowTypeError(function() { propertyIsEnumerable("s"); });
expectThrowTypeError(function() { propertyIsEnumerable.call(null, "s"); });
expectThrowTypeError(function() { propertyIsEnumerable.call(undefined, "s"); });
expectThrowTypeError(function() { propertyIsEnumerable(true); });
expectThrowTypeError(function() { propertyIsEnumerable.call(null, true); });
expectThrowTypeError(function() { propertyIsEnumerable.call(undefined, true); });
expectThrowTypeError(function() { propertyIsEnumerable(NaN); });
expectThrowTypeError(function() { propertyIsEnumerable.call(null, NaN); });
expectThrowTypeError(function() { propertyIsEnumerable.call(undefined, NaN); });

expectThrowTypeError(function() { propertyIsEnumerable({}); });
expectThrowTypeError(function() { propertyIsEnumerable.call(null, {}); });
expectThrowTypeError(function() { propertyIsEnumerable.call(undefined, {}); });

/*
 * 3. Let desc be the result of calling the [[GetOwnProperty]] internal method
 *    of O passing P as the argument.
 * 4. If desc is undefined, return false.
 */
assertEq(propertyIsEnumerable.call({}, "valueOf"), false);
assertEq(propertyIsEnumerable.call({}, "toString"), false);
assertEq(propertyIsEnumerable.call("s", 1), false);
assertEq(propertyIsEnumerable.call({}, "dsfiodjfs"), false);
assertEq(propertyIsEnumerable.call(true, "toString"), false);
assertEq(propertyIsEnumerable.call({}, "__proto__"), false);

assertEq(propertyIsEnumerable.call(Object, "getOwnPropertyDescriptor"), false);
assertEq(propertyIsEnumerable.call(this, "expectThrowTypeError"), true);
assertEq(propertyIsEnumerable.call("s", "length"), false);
assertEq(propertyIsEnumerable.call("s", 0), true);
assertEq(propertyIsEnumerable.call(Number, "MAX_VALUE"), false);
assertEq(propertyIsEnumerable.call({ x: 9 }, "x"), true);
assertEq(propertyIsEnumerable.call(function() { }, "prototype"), false);
assertEq(propertyIsEnumerable.call(function() { }, "length"), false);
assertEq(propertyIsEnumerable.call(function() { "use strict"; }, "caller"), false);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
