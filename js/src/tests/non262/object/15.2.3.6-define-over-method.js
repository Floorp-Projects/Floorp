// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 568786;
var summary =
  'Do not assert: !(attrs & (JSPROP_GETTER | JSPROP_SETTER)) with ' +
  'Object.defineProperty';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o = { x: function(){} };
Object.defineProperty(o, "x", { get: function(){} });

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
