/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 668024;
var summary =
  'Array.prototype.splice should define, not set, the elements of the array ' +
  'it returns';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

Object.defineProperty(Object.prototype, 2,
  {
    set: function(v)
    {
      throw new Error("setter on Object.prototype called!");
    },
    get: function() { return "fnord"; },
    enumerable: false,
    configurable: true
  });

var arr = [0, 1, 2, 3, 4, 5];
var removed = arr.splice(0, 6);

assertEq(arr.length, 0);
assertEq(removed.length, 6);
assertEq(removed[0], 0);
assertEq(removed[1], 1);
assertEq(removed[2], 2);
assertEq(removed[3], 3);
assertEq(removed[4], 4);
assertEq(removed[5], 5);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
