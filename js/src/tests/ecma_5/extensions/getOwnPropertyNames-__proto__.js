/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 690031;
var summary =
  'Exclude __proto__ from showing up when enumerating properties of ' +
  'Object.prototype again';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var keys = Object.getOwnPropertyNames(Object.prototype);
assertEq(keys.indexOf("__proto__"), -1,
         "shouldn't have gotten __proto__ as a property of Object.prototype " +
         "(got these properties: " + keys + ")");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
