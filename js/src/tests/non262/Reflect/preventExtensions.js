/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Reflect.preventExtensions is the same as Object.preventExtensions, except
// for the return value and the behavior in error cases.

var someObjects = [
    {},
    new Int32Array(7),
    Object(Symbol("table")),
    new Proxy({}, {})
];

for (var obj of someObjects) {
    assertEq(Reflect.preventExtensions(obj), true);
    // [[PreventExtensions]] on an already-inextensible object is a no-op.
    assertEq(Reflect.preventExtensions(obj), true);
}

// Error cases.
assertThrowsInstanceOf(() => Reflect.isExtensible(), TypeError);
for (var value of [undefined, null, true, 1, NaN, "Phaedo", Symbol("good")]) {
    assertThrowsInstanceOf(() => Reflect.isExtensible(value), TypeError);
}

// A proxy's preventExtensions handler can return false without doing anything.
obj = {};
var proxy = new Proxy(obj, {
    preventExtensions() { return false; }
});
assertEq(Reflect.preventExtensions(proxy), false);
assertEq(Reflect.isExtensible(obj), true);
assertEq(Reflect.isExtensible(proxy), true);

// If a proxy's preventExtensions handler throws, the exception is propagated.
obj = {};
proxy = new Proxy(obj, {
    preventExtensions() { throw "fit"; }
});
assertThrowsValue(() => Reflect.preventExtensions(proxy), "fit");
assertEq(Reflect.isExtensible(obj), true);
assertEq(Reflect.isExtensible(proxy), true);

// If a proxy's preventExtensions handler returns true while leaving the target
// extensible, that's a TypeError.
obj = {};
proxy = new Proxy(obj, {
    preventExtensions() { return true; }
});
assertThrowsInstanceOf(() => Reflect.preventExtensions(proxy), TypeError);
assertEq(Reflect.isExtensible(obj), true);
assertEq(Reflect.isExtensible(proxy), true);

// For more Reflect.preventExtensions tests, see target.js.

reportCompare(0, 0);
