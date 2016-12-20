/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = "global-property.js";
var BUGNUMBER = 1317422;
var summary =
  "The global object should have a non-enumerable, writable, configurable " +
  "value property whose value is the global object";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(global, this);

global = 42;
assertEq(global, 42);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
