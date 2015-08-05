/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Reflect.setPrototypeOf changes an object's [[Prototype]].
var obj = {};
assertEq(Object.getPrototypeOf(obj), Object.prototype);
var proto = {};
assertEq(Reflect.setPrototypeOf(obj, proto), true);
assertEq(Object.getPrototypeOf(obj), proto);

// It can change an object's [[Prototype]] to null.
obj = {};
assertEq(Reflect.setPrototypeOf(obj, null), true);
assertEq(Object.getPrototypeOf(obj), null);

// The proto argument is required too.
obj = {};
assertThrowsInstanceOf(() => Reflect.setPrototypeOf(obj), TypeError);

// The proto argument must be either null or an object.
for (proto of [undefined, false, 0, 1.6, "that", Symbol.iterator]) {
    assertThrowsInstanceOf(() => Reflect.setPrototypeOf(obj, proto), TypeError);
}

// Return false if the target is inextensible.
proto = {};
obj = Object.preventExtensions(Object.create(proto));
assertEq(Reflect.setPrototypeOf(obj, {}), false);
assertEq(Reflect.setPrototypeOf(obj, null), false);
assertEq(Reflect.setPrototypeOf(obj, proto), true);  // except if not changing anything

// Return false rather than create a [[Prototype]] cycle.
obj = {};
assertEq(Reflect.setPrototypeOf(obj, obj), false);

// Don't create a [[Prototype]] cycle involving 2 objects.
obj = Object.create(proto);
assertEq(Reflect.setPrototypeOf(proto, obj), false);

// Don't create a longish [[Prototype]] cycle.
for (var i = 0; i < 256; i++)
    obj = Object.create(obj);
assertEq(Reflect.setPrototypeOf(proto, obj), false);

// The spec claims we should allow creating cycles involving proxies. (The
// cycle check quietly exits on encountering the proxy.)
obj = {};
var proxy = new Proxy(Object.create(obj), {});
if (Reflect.setPrototypeOf(obj, proxy) !== false) {
    throw new Error("Congratulations on implementing ES6 [[SetPrototype]]! " +
                    "Update this test for 1 karma point!");
    // ...by deleting this if-block and uncommenting the three assertions below.
}
// assertEq(Reflect.setPrototypeOf(obj, proxy), true);
// assertEq(Reflect.getPrototypeOf(obj), proxy);
// assertEq(Reflect.getPrototypeOf(proxy), obj);

// If a proxy handler returns a false-y value, return false.
var hits = 0;
proto = {name: "proto"};
obj = {name: "obj"};
proxy = new Proxy(obj, {
    setPrototypeOf(t, p) {
        assertEq(t, obj);
        assertEq(p, proto);
        hits++;
        return 0;
    }
});
if (Reflect.setPrototypeOf(proxy, proto) !== true) {
    throw new Error("Congratulations on implementing the setPrototypeOf trap for proxies! " +
                    "Please update this test.");
    // ...by deleting this if-block and uncommenting the two assertions below.
    // As of this writing, the setPrototypeOf hook is never called; see bug 888969.
}
// assertEq(Reflect.setPrototypeOf(proxy, proto), false);
// assertEq(hits, 1);

// For more Reflect.setPrototypeOf tests, see target.js.

reportCompare(0, 0);
