/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Reflect.deleteProperty deletes properties.
var obj = {x: 1, y: 2};
assertEq(Reflect.deleteProperty(obj, "x"), true);
assertDeepEq(obj, {y: 2});

var arr = [1, 1, 2, 3, 5];
assertEq(Reflect.deleteProperty(arr, "3"), true);
assertDeepEq(arr, [1, 1, 2, , 5]);


// === Failure and error cases
// Since Reflect.deleteProperty is almost exactly identical to the non-strict
// `delete` operator, there is not much to test that would not be redundant.

// Returns true if no such property exists.
assertEq(Reflect.deleteProperty({}, "q"), true);

// Or if it's inherited.
var proto = {x: 1};
assertEq(Reflect.deleteProperty(Object.create(proto), "x"), true);
assertEq(proto.x, 1);

// Return false if asked to delete a non-configurable property.
var arr = [];
assertEq(Reflect.deleteProperty(arr, "length"), false);
assertEq(arr.hasOwnProperty("length"), true);
assertEq(Reflect.deleteProperty(this, "undefined"), false);
assertEq(this.undefined, void 0);

// Return false if a Proxy's deleteProperty handler returns a false-y value.
var value;
var proxy = new Proxy({}, {
    deleteProperty(t, k) {
        return value;
    }
});
for (value of [true, false, 0, "something", {}]) {
    assertEq(Reflect.deleteProperty(proxy, "q"), !!value);
}

// If a Proxy's handler method throws, the error is propagated.
proxy = new Proxy({}, {
    deleteProperty(t, k) { throw "vase"; }
});
assertThrowsValue(() => Reflect.deleteProperty(proxy, "prop"), "vase");

// Throw a TypeError if a Proxy's handler method returns true in violation of
// the object invariants.
proxy = new Proxy(Object.freeze({prop: 1}), {
    deleteProperty(t, k) { return true; }
});
assertThrowsInstanceOf(() => Reflect.deleteProperty(proxy, "prop"), TypeError);


// === Deleting elements from `arguments`

// Non-strict arguments element becomes unmapped
function f(x, y, z) {
    assertEq(Reflect.deleteProperty(arguments, "0"), true);
    arguments.x = 33;
    return x;
}
assertEq(f(17, 19, 23), 17);

// Frozen non-strict arguments element
function testFrozenArguments() {
    Object.freeze(arguments);
    assertEq(Reflect.deleteProperty(arguments, "0"), false);
    assertEq(arguments[0], "zero");
    assertEq(arguments[1], "one");
}
testFrozenArguments("zero", "one");


// For more Reflect.deleteProperty tests, see target.js and propertyKeys.js.

reportCompare(0, 0);
