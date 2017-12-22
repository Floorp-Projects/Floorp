/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 492840;
var summary = 'ES5 Object.create(O [, Properties])';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq("create" in Object, true);
assertEq(Object.create.length, 2);

var o, desc, props, proto;

o = Object.create(null);
assertEq(Object.getPrototypeOf(o), null, "bad null-proto");

o = Object.create(null, { a: { value: 17, enumerable: false } });
assertEq(Object.getPrototypeOf(o), null, "bad null-proto");
assertEq("a" in o, true);
desc = Object.getOwnPropertyDescriptor(o, "a");
assertEq(desc !== undefined, true, "no descriptor?");
assertEq(desc.value, 17);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
assertEq(desc.writable, false);

props = Object.create({ bar: 15 });
Object.defineProperty(props, "foo", { enumerable: false, value: 42 });
proto = { baz: 12 };
o = Object.create(proto, props);
assertEq(Object.getPrototypeOf(o), proto);
assertEq(Object.getOwnPropertyDescriptor(o, "foo"), undefined);
assertEq("foo" in o, false);
assertEq(Object.getOwnPropertyDescriptor(o, "bar"), undefined);
assertEq("bar" in o, false);
assertEq(Object.getOwnPropertyDescriptor(o, "baz"), undefined);
assertEq(o.baz, 12);
assertEq(o.hasOwnProperty("baz"), false);

try {
  var actual =
    Object.create(Object.create({},
                                { boom: { get: function() { return "base"; }}}),
                  { boom: { get: function() { return "overridden"; }}}).boom
} catch (e) {
}
assertEq(actual, "overridden");

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");
