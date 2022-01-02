/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 677820;
var summary =
  "String.prototype.match must define matches on the returned array, not set " +
  "them";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var called = false;
function setterFunction(v) { called = true; }
function getterFunction(v) { return "getter"; }

Object.defineProperty(Array.prototype, 1,
  { get: getterFunction, set: setterFunction });

assertEq(called, false);
var matches = "abcdef".match(/./g);
assertEq(called, false);
assertEq(matches.length, 6);
assertEq(matches[0], "a");
assertEq(matches[1], "b");
assertEq(matches[2], "c");
assertEq(matches[3], "d");
assertEq(matches[4], "e");
assertEq(matches[5], "f");

var desc = Object.getOwnPropertyDescriptor(Array.prototype, 1);
assertEq(desc.get, getterFunction);
assertEq(desc.set, setterFunction);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
assertEq([][1], "getter");

assertEq(called, false);

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
