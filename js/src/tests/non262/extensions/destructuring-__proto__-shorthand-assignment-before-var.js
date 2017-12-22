/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'destructuring-__proto__-shorthand-assignment-before-var.js';
var BUGNUMBER = 963641;
var summary = "{ __proto__ } should work as a destructuring assignment pattern";

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

({ __proto__ } = objectWithProtoProperty(17));
assertEq(__proto__, 17);

var { __proto__ } = objectWithProtoProperty(42);
assertEq(__proto__, 42);

function nested()
{
  ({ __proto__ } = objectWithProtoProperty(undefined));
  assertEq(__proto__, undefined);

  var { __proto__ } = objectWithProtoProperty("fnord");
  assertEq(__proto__, "fnord");
}
nested();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
