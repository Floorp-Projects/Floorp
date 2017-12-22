/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'destructuring-for-inof-__proto__.js';
var BUGNUMBER = 963641;
var summary =
  "__proto__ should work in destructuring patterns as the targets of " +
  "for-in/for-of loops";

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

function* objectWithProtoGenerator(v)
{
  yield objectWithProtoProperty(v);
}

function* identityGenerator(v)
{
  yield v;
}

for (var { __proto__: target } of objectWithProtoGenerator(null))
  assertEq(target, null);

for ({ __proto__: target } of objectWithProtoGenerator("aacchhorrt"))
  assertEq(target, "aacchhorrt");

for ({ __proto__: target } of identityGenerator(42))
  assertEq(target, Number.prototype);

for (var { __proto__: target } in { prop: "kneedle" })
  assertEq(target, String.prototype);

for ({ __proto__: target } in { prop: "snork" })
  assertEq(target, String.prototype);

for ({ __proto__: target } in { prop: "ohia" })
  assertEq(target, String.prototype);

function nested()
{
  for (var { __proto__: target } of objectWithProtoGenerator(null))
    assertEq(target, null);

  for ({ __proto__: target } of objectWithProtoGenerator("aacchhorrt"))
    assertEq(target, "aacchhorrt");

  for ({ __proto__: target } of identityGenerator(42))
    assertEq(target, Number.prototype);

  for (var { __proto__: target } in { prop: "kneedle" })
    assertEq(target, String.prototype);

  for ({ __proto__: target } in { prop: "snork" })
    assertEq(target, String.prototype);

  for ({ __proto__: target } in { prop: "ohia" })
    assertEq(target, String.prototype);
}
nested();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
