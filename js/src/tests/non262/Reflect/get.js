/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

// Reflect.get gets the value of a property.

var x = {p: 1};
assertEq(Reflect.get(x, "p"), 1);
assertEq(Reflect.get(x, "toString"), Object.prototype.toString);
assertEq(Reflect.get(x, "missing"), undefined);


// === Various targets

// Array
assertEq(Reflect.get([], 700), undefined);
assertEq(Reflect.get(["zero", "one"], 1), "one");

// TypedArray
assertEq(Reflect.get(new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7]), 7), 7);

// Treatment of NaN
var f = new Float64Array([NaN]);
var u = new Uint32Array(f.buffer);
u[0]++;
u[1]++;
assertEq(f[0], NaN);
assertEq(Reflect.get(f, 0), NaN);

// Proxy with no get handler
assertEq(Reflect.get(new Proxy(x, {}), "p"), 1);

// Proxy with a get handler
var obj = new Proxy(x, {
    get(t, k, r) { return k + "ful"; }
});
assertEq(Reflect.get(obj, "mood"), "moodful");

// Exceptions thrown by a proxy's get handler are propagated.
assertThrowsInstanceOf(() => Reflect.get(obj, Symbol()), TypeError);

// Ordinary object, property has a setter and no getter
obj = {set name(x) {}};
assertEq(Reflect.get(obj, "name"), undefined);


// === Receiver

// Receiver argument is passed to getters as the this-value.
obj = { get x() { return this; }};
assertEq(Reflect.get(obj, "x", Math), Math);
assertEq(Reflect.get(Object.create(obj), "x", JSON), JSON);

// If missing, target is passed instead.
assertEq(Reflect.get(obj, "x"), obj);

// Receiver argument is passed to the proxy get handler.
obj = new Proxy({}, {
    get(t, k, r) {
        assertEq(k, "itself");
        return r;
    }
});
assertEq(Reflect.get(obj, "itself"), obj);
assertEq(Reflect.get(obj, "itself", Math), Math);
assertEq(Reflect.get(Object.create(obj), "itself", Math), Math);

// The receiver shouldn't have to be an object
assertEq(Reflect.get(obj, "itself", 37.2), 37.2);

// For more Reflect.get tests, see target.js and propertyKeys.js.

reportCompare(0, 0);
