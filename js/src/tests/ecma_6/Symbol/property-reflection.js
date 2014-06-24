/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Basic tests for standard Object APIs interacting with symbols.

// Object.defineProperty
function F() {}
var f = new F;
Object.defineProperty(f, Symbol.for("name"), {
    configurable: true,
    value: "eff"
});
assertEq("name" in f, false);
assertEq("Symbol(name)" in f, false);
assertEq(Symbol.for("name") in f, true);
assertEq(f[Symbol.for("name")], "eff");

// Object.defineProperties
function D() {}
var descs = new D;
var s1 = Symbol("s1");
var hits = 0;
descs[s1] = {
    configurable: true,
    enumerable: true,
    get: () => hits++,
    set: undefined
};
var s2 = Symbol("s2");
descs[s2] = {
    configurable: true,
    enumerable: false,
    value: {},
    writable: true
};
var s3 = Symbol("s3");
D.prototype[s3] = {value: "FAIL"};
assertEq(Object.defineProperties(f, descs), f);
assertEq(s1 in f, true);
assertEq(f[s1], 0);
assertEq(hits, 1);
assertEq(s2 in f, true);
assertEq(f[s2], descs[s2].value);
assertEq(s3 in f, false);

// Object.create
var n = Object.create({}, descs);
assertEq(s1 in n, true);
assertEq(n[s1], 1);
assertEq(hits, 2);
assertEq(s2 in n, true);
assertEq(n[s2], descs[s2].value);
assertEq(s3 in n, false);

// Object.getOwnPropertyDescriptor
var desc = Object.getOwnPropertyDescriptor(n, s1);
assertDeepEq(desc, descs[s1]);
assertEq(desc.get, descs[s1].get);
desc = Object.getOwnPropertyDescriptor(n, s2);
assertDeepEq(desc, descs[s2]);
assertEq(desc.value, descs[s2].value);

// Object.prototype.hasOwnProperty
assertEq(descs.hasOwnProperty(s1), true);
assertEq(descs.hasOwnProperty(s2), true);
assertEq(descs.hasOwnProperty(s3), false);
assertEq([].hasOwnProperty(Symbol.iterator), false);
if (!("@@iterator" in []))
    throw new Error("Congratulations on implementing Symbol.iterator! Please update this test.");
assertEq(Array.prototype.hasOwnProperty(Symbol.iterator), false);  // should be true

// Object.prototype.propertyIsEnumerable
assertEq(n.propertyIsEnumerable(s1), true);
assertEq(n.propertyIsEnumerable(s2), false);
assertEq(n.propertyIsEnumerable(s3), false);  // no such property
assertEq(D.prototype.propertyIsEnumerable(s3), true);
assertEq(descs.propertyIsEnumerable(s3), false); // inherited properties are not considered

// Object.preventExtensions
var obj = {};
obj[s1] = 1;
assertEq(Object.preventExtensions(obj), obj);
assertThrowsInstanceOf(function () { "use strict"; obj[s2] = 2; }, TypeError);
obj[s2] = 2;  // still no effect
assertEq(s2 in obj, false);

// Object.isSealed, Object.isFrozen
assertEq(Object.isSealed(obj), false);
assertEq(Object.isFrozen(obj), false);
assertEq(delete obj[s1], true);
assertEq(Object.isSealed(obj), true);
assertEq(Object.isFrozen(obj), true);

obj = {};
obj[s1] = 1;
Object.preventExtensions(obj);
Object.defineProperty(obj, s1, {configurable: false});  // still writable
assertEq(Object.isSealed(obj), true);
assertEq(Object.isFrozen(obj), false);
obj[s1] = 2;
assertEq(obj[s1], 2);
Object.defineProperty(obj, s1, {writable: false});
assertEq(Object.isFrozen(obj), true);

// Object.seal, Object.freeze
var obj = {};
obj[s1] = 1;
Object.seal(obj);
desc = Object.getOwnPropertyDescriptor(obj, s1);
assertEq(desc.configurable, false);
assertEq(desc.writable, true);
Object.freeze(obj);
assertEq(Object.getOwnPropertyDescriptor(obj, s1).writable, false);

// Object.setPrototypeOf purges caches for symbol-keyed properties.
var proto = {};
proto[s1] = 1;
Object.defineProperty(proto, s2, {
    get: () => 2,
    set: v => undefined
});
var obj = Object.create(proto);
var last1, last2;
var N = 9;
for (var i = 0; i < N; i++) {
    last1 = obj[s1];
    last2 = obj[s2];
    obj[s2] = "marker";
    if (i === N - 2)
        Object.setPrototypeOf(obj, {});
}
assertEq(last1, undefined);
assertEq(last2, undefined);
assertEq(obj.hasOwnProperty(s2), true);
assertEq(obj[s2], "marker");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
