/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Reflect.has is identical to the `in` operator.
assertEq(Reflect.has({x: 0}, "x"), true);
assertEq(Reflect.has({x: 0}, "y"), false);
assertEq(Reflect.has({x: 0}, "toString"), true);

// The target can be an array; Reflect.has works on array elements.
var arr = ["zero"];
arr[10000] = 0;
assertEq(Reflect.has(arr, "10000"), true);
assertEq(Reflect.has(arr, 10000), true);
assertEq(Reflect.has(arr, "-0"), false);
assertEq(Reflect.has(arr, -0), true);

// And string objects (though not string primitives; see target.js).
var str = new String("hello");
assertEq(Reflect.has(str, "4"), true);
assertEq(Reflect.has(str, "-0"), false);
assertEq(Reflect.has(str, -0), true);

// Proxy without .has() handler method
var obj = {get prop() {}};
for (var i = 0; i < 2; i++) {
    obj = new Proxy(obj, {});
    assertEq(Reflect.has(obj, "prop"), true);
    assertEq(Reflect.has(obj, "nope"), false);
}

// Proxy with .has() handler method
obj = new Proxy({}, {
    has(t, k) { return k.startsWith("door"); }
});
assertEq(Reflect.has(obj, "doorbell"), true);
assertEq(Reflect.has(obj, "dormitory"), false);


// For more Reflect.has tests, see target.js and propertyKeys.js.

reportCompare(0, 0);
