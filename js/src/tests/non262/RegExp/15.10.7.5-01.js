/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 465199;
var summary = "RegExp lastIndex property set should not coerce type to number";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var called = false;
var o = { valueOf: function() { called = true; return 1; } };
var r = /a/g;
var desc, m;

assertEq(r.lastIndex, 0);

desc = Object.getOwnPropertyDescriptor(r, "lastIndex");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
assertEq(desc.value, 0);
assertEq(desc.writable, true);

r.lastIndex = o;

assertEq(r.lastIndex, o);

desc = Object.getOwnPropertyDescriptor(r, "lastIndex");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
assertEq(desc.value, o);
assertEq(desc.writable, true);

assertEq(called, false);
assertEq(r.exec("aaaa").length, 1);
assertEq(called, true);
assertEq(r.lastIndex, 2);

desc = Object.getOwnPropertyDescriptor(r, "lastIndex");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
assertEq(desc.value, 2);
assertEq(desc.writable, true);


r.lastIndex = -0.2;
assertEq(r.lastIndex, -0.2);

m = r.exec("aaaa");
assertEq(Array.isArray(m), true);
assertEq(m.length, 1);
assertEq(m[0], "a");
assertEq(r.lastIndex, 1);

m = r.exec("aaaa");
assertEq(Array.isArray(m), true);
assertEq(m.length, 1);
assertEq(m[0], "a");
assertEq(r.lastIndex, 2);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
