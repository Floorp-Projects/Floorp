// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = '15.2.3.6-miscellaneous.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 430133;
var summary = 'ES5 Object.defineProperty(O, P, Attributes)';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o = [];
Object.defineProperty(o, 0, { value: 17 });
var desc = Object.getOwnPropertyDescriptor(o, 0);
assertEq(desc !== undefined, true);
assertEq(desc.value, 17);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
assertEq(desc.writable, false);

desc = Object.getOwnPropertyDescriptor(o, "length");
assertEq(desc !== undefined, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
assertEq(desc.writable, true);
assertEq(desc.value, 1);
assertEq(o.length, 1);

Object.defineProperty(o, "foobar",
                      { value: 42, enumerable: false, configurable: true });
assertEq(o.foobar, 42);
desc = Object.getOwnPropertyDescriptor(o, "foobar");
assertEq(desc !== undefined, true);
assertEq(desc.value, 42);
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);
assertEq(desc.writable, false);

var called = false;
o = { set x(a) { called = true; } };
Object.defineProperty(o, "x", { get: function() { return "get"; } });
desc = Object.getOwnPropertyDescriptor(o, "x");
assertEq("set" in desc, true);
assertEq("get" in desc, true);
assertEq(called, false);
o.x = 13;
assertEq(called, true);

var toSource = Object.prototype.toSource || function() { };
toSource.call(o); // a test for this not crashing

var called = false;
var o =
  Object.defineProperty({}, "foo",
                        { get: function() { return 17; },
                          set: function(v) { called = true; } });

assertEq(o.foo, 17);
o.foo = 42;
assertEq(called, true);

/*
 * XXX need tests for Object.defineProperty(array, "length", { ... }) when we
 * support it!
 */

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
