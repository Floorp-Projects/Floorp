/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'getPrototypeOf-array.js';
var BUGNUMBER = 769041;
var summary =
  "The [[Prototype]] of an object whose prototype chain contains an array " +
  "isn't that array's [[Prototype]]";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr = [];
assertEq(Array.isArray(arr), true);
var objWithArrPrototype = Object.create(arr);
assertEq(!Array.isArray(objWithArrPrototype), true);
assertEq(Object.getPrototypeOf(objWithArrPrototype), arr);
var objWithArrGrandPrototype = Object.create(objWithArrPrototype);
assertEq(!Array.isArray(objWithArrGrandPrototype), true);
assertEq(Object.getPrototypeOf(objWithArrGrandPrototype), objWithArrPrototype);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
