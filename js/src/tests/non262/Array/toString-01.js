/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 562446;
var summary = 'ES5: Array.prototype.toString';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o;

o = { join: function() { assertEq(arguments.length, 0); return "ohai"; } };
assertEq(Array.prototype.toString.call(o), "ohai");

o = {};
assertEq(Array.prototype.toString.call(o), "[object Object]");

Array.prototype.join = function() { return "kthxbai"; };
assertEq(Array.prototype.toString.call([]), "kthxbai");

o = { join: 17 };
assertEq(Array.prototype.toString.call(o), "[object Object]");

o = { get join() { throw 42; } };
try
{
  var str = Array.prototype.toString.call(o);
  assertEq(true, false,
           "expected an exception calling [].toString on an object with a " +
           "join getter that throws, got " + str + " instead");
}
catch (e)
{
  assertEq(e, 42,
           "expected thrown e === 42 when calling [].toString on an object " +
           "with a join getter that throws, got " + e);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
