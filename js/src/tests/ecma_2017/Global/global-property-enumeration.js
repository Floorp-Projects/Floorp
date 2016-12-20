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

var keys = Object.keys(this);
assertEq(keys.indexOf("global"), -1);

var props = Object.getOwnPropertyNames(this);
assertEq(props.indexOf("global") >= 0, true);

var desc = Object.getOwnPropertyDescriptor(this, "global");
assertEq(desc !== undefined, true);

assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(desc.writable, true);
assertEq(desc.value, this);

global = 42;
assertEq(this.global, 42);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
