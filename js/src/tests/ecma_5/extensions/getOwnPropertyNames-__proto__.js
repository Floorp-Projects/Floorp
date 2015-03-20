/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 837630;
var summary ='__proto__ should show up with O.getOwnPropertyNames(O.prototype)';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var keys = Object.getOwnPropertyNames(Object.prototype);
assertEq(keys.indexOf("__proto__") >= 0, true,
         "should have gotten __proto__ as a property of Object.prototype " +
         "(got these properties: " + keys + ")");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
