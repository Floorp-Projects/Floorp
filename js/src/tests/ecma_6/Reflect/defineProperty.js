/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Reflect.defineProperty defines properties.
var obj = {};
assertEq(Reflect.defineProperty(obj, "x", {value: 7}), true);
assertEq(obj.x, 7);
var desc = Reflect.getOwnPropertyDescriptor(obj, "x");
assertDeepEq(desc, {value: 7,
                    writable: false,
                    enumerable: false,
                    configurable: false});

// Reflect.defineProperty can define a symbol-keyed property.
var key = Symbol(":o)");
assertEq(Reflect.defineProperty(obj, key, {value: 8}), true);
assertEq(obj[key], 8);

// array .length property
obj = [1, 2, 3, 4, 5];
assertEq(Reflect.defineProperty(obj, "length", {value: 4}), true);
assertDeepEq(obj, [1, 2, 3, 4]);

// The target can be a proxy.
obj = {};
var proxy = new Proxy(obj, {
    defineProperty(t, id, desc) {
        t[id] = 1;
        return true;
    }
});
assertEq(Reflect.defineProperty(proxy, "prop", {value: 7}), true);
assertEq(obj.prop, 1);
assertEq(delete obj.prop, true);
assertEq("prop" in obj, false);

// The attributes object is re-parsed, not passed through to the
// handler.defineProperty method.
obj = {};
var attributes = {
    configurable: 17,
    enumerable: undefined,
    value: null
};
proxy = new Proxy(obj, {
    defineProperty(t, id, desc) {
        assertEq(desc !== attributes, true);
        assertEq(desc.configurable, true);
        assertEq(desc.enumerable, false);
        assertEq(desc.value, null);
        assertEq("writable" in desc, false);
        return 15;  // and the return value here is coerced to boolean
    }
});
assertEq(Reflect.defineProperty(proxy, "prop", attributes), true);


// === Failure and error cases
//
// Reflect.defineProperty behaves much like Object.defineProperty, which has
// extremely thorough tests elsewhere, and the implementation is largely
// shared. Duplicating those tests with Reflect.defineProperty would be a
// big waste.
//
// However, certain failures cause Reflect.defineProperty to return false
// without throwing a TypeError (unlike Object.defineProperty). So here we test
// many error cases to check that behavior.

// missing attributes argument
assertThrowsInstanceOf(() => Reflect.defineProperty(obj, "y"),
                       TypeError);

// non-object attributes argument
for (var attributes of SOME_PRIMITIVE_VALUES) {
    assertThrowsInstanceOf(() => Reflect.defineProperty(obj, "y", attributes),
                           TypeError);
}

// inextensible object
obj = Object.preventExtensions({});
assertEq(Reflect.defineProperty(obj, "prop", {value: 4}), false);

// inextensible object with irrelevant inherited property
obj = Object.preventExtensions(Object.create({"prop": 3}));
assertEq(Reflect.defineProperty(obj, "prop", {value: 4}), false);

// redefine nonconfigurable to configurable
obj = Object.freeze({prop: 1});
assertEq(Reflect.defineProperty(obj, "prop", {configurable: true}), false);

// redefine enumerability of nonconfigurable property
obj = Object.freeze(Object.defineProperties({}, {
    x: {enumerable: true,  configurable: false, value: 0},
    y: {enumerable: false, configurable: false, value: 0},
}));
assertEq(Reflect.defineProperty(obj, "x", {enumerable: false}), false);
assertEq(Reflect.defineProperty(obj, "y", {enumerable: true}), false);

// redefine nonconfigurable data to accessor property, or vice versa
obj = Object.seal({x: 1, get y() { return 2; }});
assertEq(Reflect.defineProperty(obj, "x", {get() { return 2; }}), false);
assertEq(Reflect.defineProperty(obj, "y", {value: 1}), false);

// redefine nonwritable, nonconfigurable property as writable
obj = Object.freeze({prop: 0});
assertEq(Reflect.defineProperty(obj, "prop", {writable: true}), false);
assertEq(Reflect.defineProperty(obj, "prop", {writable: false}), true);  // no-op

// change value of nonconfigurable nonwritable property
obj = Object.freeze({prop: 0});
assertEq(Reflect.defineProperty(obj, "prop", {value: -0}), false);
assertEq(Reflect.defineProperty(obj, "prop", {value: +0}), true);  // no-op

// change getter or setter
function g() {}
function s(x) {}
obj = {};
Object.defineProperty(obj, "prop", {get: g, set: s, configurable: false});
assertEq(Reflect.defineProperty(obj, "prop", {get: s}), false);
assertEq(Reflect.defineProperty(obj, "prop", {get: g}), true);  // no-op
assertEq(Reflect.defineProperty(obj, "prop", {set: g}), false);
assertEq(Reflect.defineProperty(obj, "prop", {set: s}), true);  // no-op

// Proxy defineProperty handler method that returns false
var falseValues = [false, 0, -0, "", NaN, null, undefined];
if (typeof objectEmulatingUndefined === "function")
    falseValues.push(objectEmulatingUndefined());
var value;
proxy = new Proxy({}, {
    defineProperty(t, id, desc) {
        return value;
    }
});
for (value of falseValues) {
    assertEq(Reflect.defineProperty(proxy, "prop", {value: 1}), false);
}

// Proxy defineProperty handler method returns true, in violation of invariants.
// Per spec, this is a TypeError, not a false return.
obj = Object.freeze({x: 1});
proxy = new Proxy(obj, {
    defineProperty(t, id, desc) {
        return true;
    }
});
assertThrowsInstanceOf(() => Reflect.defineProperty(proxy, "x", {value: 2}), TypeError);
assertThrowsInstanceOf(() => Reflect.defineProperty(proxy, "y", {value: 0}), TypeError);
assertEq(Reflect.defineProperty(proxy, "x", {value: 1}), true);

// The second argument is converted ToPropertyKey before any internal methods
// are called on the first argument.
var poison =
  (counter => new Proxy({}, new Proxy({}, { get() { throw counter++; } })))(42);
assertThrowsValue(() => {
    Reflect.defineProperty(poison, {
        toString() { throw 17; },
        valueOf() { throw 8675309; }
    }, poison);
}, 17);


// For more Reflect.defineProperty tests, see target.js and propertyKeys.js.

reportCompare(0, 0);
