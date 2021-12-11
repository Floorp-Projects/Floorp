/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'toLocaleString.js';
var BUGNUMBER = 653789;
var summary = "Object.prototype.toLocaleString";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function expectThrowTypeError(fun)
{
  try
  {
    var r = fun();
    throw "didn't throw TypeError, returned " + r;
  }
  catch (e)
  {
    assertEq(e instanceof TypeError, true,
             "didn't throw TypeError, got: " + e);
  }
}

var toLocaleString = Object.prototype.toLocaleString;

/*
 * 1. Let O be the result of calling ToObject passing the this value as the
 *    argument.
 */
expectThrowTypeError(function() { toLocaleString.call(null); });
expectThrowTypeError(function() { toLocaleString.call(undefined); });
expectThrowTypeError(function() { toLocaleString.apply(null); });
expectThrowTypeError(function() { toLocaleString.apply(undefined); });


/*
 * 2. Let toString be the result of calling the [[Get]] internal method of O
 *    passing "toString" as the argument.
 */
try
{
  toLocaleString.call({ get toString() { throw 17; } });
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e, 17);
}


/* 3. If IsCallable(toString) is false, throw a TypeError exception. */
expectThrowTypeError(function() { toLocaleString.call({ toString: 12 }); });
expectThrowTypeError(function() { toLocaleString.call({ toString: 0.3423423452352e9 }); });
expectThrowTypeError(function() { toLocaleString.call({ toString: undefined }); });
expectThrowTypeError(function() { toLocaleString.call({ toString: false }); });
expectThrowTypeError(function() { toLocaleString.call({ toString: [] }); });
expectThrowTypeError(function() { toLocaleString.call({ toString: {} }); });
expectThrowTypeError(function() { toLocaleString.call({ toString: new String }); });
expectThrowTypeError(function() { toLocaleString.call({ toString: new Number(7.7) }); });
expectThrowTypeError(function() { toLocaleString.call({ toString: new Boolean(true) }); });
expectThrowTypeError(function() { toLocaleString.call({ toString: JSON }); });

expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: 12 }); });
expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: 0.3423423452352e9 }); });
expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: undefined }); });
expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: false }); });
expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: [] }); });
expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: {} }); });
expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: new String }); });
expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: new Number(7.7) }); });
expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: new Boolean(true) }); });
expectThrowTypeError(function() { toLocaleString.call({ valueOf: 0, toString: JSON }); });


/*
 * 4. Return the result of calling the [[Call]] internal method of toString
 *    passing O as the this value and no arguments.
 */
assertEq(toLocaleString.call({ get toString() { return function() { return "foo"; } } }),
         "foo");

var obj = { toString: function() { assertEq(this, obj); assertEq(arguments.length, 0); return 5; } };
assertEq(toLocaleString.call(obj), 5);

assertEq(toLocaleString.call({ toString: function() { return obj; } }), obj);

assertEq(toLocaleString.call({ toString: function() { return obj; },
                               valueOf: function() { return "abc"; } }),
         obj);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
