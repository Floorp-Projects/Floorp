/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

assertEq(typeof Reflect, 'object');
assertEq(Object.getPrototypeOf(Reflect), Object.prototype);
assertEq(Reflect.toString(), '[object Object]');

var desc = Object.getOwnPropertyDescriptor(this, "Reflect");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(desc.writable, true);

// Assert that the SpiderMonkey "resolve hook" mechanism does not resurrect the
// Reflect property once it is deleted.
delete this.Reflect;
assertEq("Reflect" in this, false);

reportCompare(0, 0, 'ok');
