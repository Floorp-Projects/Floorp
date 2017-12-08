/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'destructuring-__proto__-target--assignment.js';
var BUGNUMBER = 963641;
var summary =
  "{ __proto__: target } should work as a destructuring assignment pattern";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function objectWithProtoProperty(v)
{
  var obj = {};
  return Object.defineProperty(obj, "__proto__",
                               {
                                 enumerable: true,
                                 configurable: true,
                                 writable: true,
                                 value: v
                               });
}

var { __proto__: target } = objectWithProtoProperty(null);
assertEq(target, null);

({ __proto__: target } = objectWithProtoProperty("aacchhorrt"));
assertEq(target, "aacchhorrt");

function nested()
{
  var { __proto__: target } = objectWithProtoProperty(3.141592654);
  assertEq(target, 3.141592654);

  ({ __proto__: target } = objectWithProtoProperty(-0));
  assertEq(target, -0);
}
nested();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
