/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = '__proto__.js';
var BUGNUMBER = 770344;
var summary = "__proto__ as accessor";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var protoDesc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
assertEq(protoDesc !== null, true);
assertEq(typeof protoDesc, "object");
assertEq(protoDesc.hasOwnProperty("get"), true);
assertEq(protoDesc.hasOwnProperty("set"), true);
assertEq(protoDesc.hasOwnProperty("enumerable"), true);
assertEq(protoDesc.hasOwnProperty("configurable"), true);
assertEq(protoDesc.hasOwnProperty("value"), false);
assertEq(protoDesc.hasOwnProperty("writable"), false);

assertEq(protoDesc.configurable, true);
assertEq(protoDesc.enumerable, false);
assertEq(typeof protoDesc.get, "function", protoDesc.get + "");
assertEq(typeof protoDesc.set, "function", protoDesc.set + "");

assertEq(delete Object.prototype.__proto__, true);
assertEq(Object.getOwnPropertyDescriptor(Object.prototype, "__proto__"),
         undefined);

var obj = {};
obj.__proto__ = 5;
assertEq(Object.getPrototypeOf(obj), Object.prototype);
assertEq(obj.hasOwnProperty("__proto__"), true);

var desc = Object.getOwnPropertyDescriptor(obj, "__proto__");
assertEq(desc !== null, true);
assertEq(typeof desc, "object");
assertEq(desc.value, 5);
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, true);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
