// Tests for TypedArray#every.
for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.every.length, 1);

    // Basic tests.
    assertEq(new constructor([1, 3, 5]).every(v => v % 2), true);
    assertEq(new constructor([1, 3, 5]).every(v => v > 2), false);
    assertEq(new constructor(10).every(v => v === 0), true);
    assertEq(new constructor().every(v => v > 1), true);

    var arr = new constructor([1, 2, 3, 4, 5]);
    var sum = 0;
    var count = 0;
    assertEq(arr.every((v, k, o) => {
        count++;
        sum += v;
        assertEq(k, v - 1);
        assertEq(o, arr);
        return v < 3;
    }), false);
    assertEq(sum, 6);
    assertEq(count, 3);

    // Tests for `thisArg` argument.
    function assertThisArg(thisArg, thisValue) {
        // In sloppy mode, `this` could be global object or a wrapper of `thisArg`.
        assertEq(arr.every(function() {
            assertDeepEq(this, thisValue);
            return true;
        }, thisArg), true);

        // In strict mode, `this` strictly equals `thisArg`.
        assertEq(arr.every(function() {
            "use strict";
            assertDeepEq(this, thisArg);
            return true;
        }, thisArg), true);

        // Passing `thisArg` has no effect if callback is an arrow function.
        var self = this;
        assertEq(arr.every(() => {
            assertEq(this, self);
            return true;
        }, thisArg), true);
    }
    assertThisArg([1, 2, 3], [1, 2, 3]);
    assertThisArg(Object, Object);
    assertThisArg(1, Object(1));
    assertThisArg("1", Object("1"));
    assertThisArg(false, Object(false));
    assertThisArg(undefined, this);
    assertThisArg(null, this);

    // Throw an exception in the callback.
    var sum = 0;
    var count = 0;
    var thrown = false;
    try {
        arr.every((v, k, o) => {
            count++;
            sum += v;
            assertEq(k, v - 1);
            assertEq(o, arr);
            if (v === 3) {
                throw "every";
            }
            return true
        })
    } catch(e) {
        assertEq(e, "every");
        thrown = true;
    }
    assertEq(thrown, true);
    assertEq(sum, 6);
    assertEq(count, 3);

    // There is no callback or callback is not a function.
    assertThrowsInstanceOf(() => {
        arr.every();
    }, TypeError);
    var invalidCallbacks = [undefined, null, 1, false, "", Symbol(), [], {}, /./];
    invalidCallbacks.forEach(callback => {
        assertThrowsInstanceOf(() => {
            arr.every(callback);
        }, TypeError);
    })

    // Callback is a generator.
    arr.every(function*(){
        throw "This line will not be executed";
    });

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var every = newGlobal()[constructor.name].prototype.every;
        var sum = 0;
        assertEq(every.call(new constructor([1, 2, 3]), v => sum += v), true);
        assertEq(sum, 6);
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.every.call(invalidReceiver, () => true);
        }, TypeError, "Assert that every fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    assertEq(Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).every(() => true), true);
}

for (let constructor of anyTypedArrayConstructors.filter(isFloatConstructor)) {
    assertEq(new constructor([undefined, , NaN]).every(v => Object.is(v, NaN)), true);
}

// Tests for TypedArray#some.
for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.some.length, 1);

    // Basic tests.
    assertEq(new constructor([1, 2, 3]).some(v => v % 2), true);
    assertEq(new constructor([0, 2, 4]).some(v => v % 2), false);
    assertEq(new constructor([1, 3, 5]).some(v => v > 2), true);
    assertEq(new constructor([1, 3, 5]).some(v => v < 0), false);
    assertEq(new constructor(10).some(v => v !== 0), false);
    assertEq(new constructor().some(v => v > 1), false);

    var arr = new constructor([1, 2, 3, 4, 5]);
    var sum = 0;
    var count = 0;
    assertEq(arr.some((v, k, o) => {
        count++;
        sum += v;
        assertEq(k, v - 1);
        assertEq(o, arr);
        return v > 2;
    }), true);
    assertEq(sum, 6);
    assertEq(count, 3);

    // Tests for `thisArg` argument.
    function assertThisArg(thisArg, thisValue) {
        // In sloppy mode, `this` could be global object or a wrapper of `thisArg`.
        assertEq(arr.some(function() {
            assertDeepEq(this, thisValue);
            return false;
        }, thisArg), false);

        // In strict mode, `this` strictly equals `thisArg`.
        assertEq(arr.some(function() {
            "use strict";
            assertDeepEq(this, thisArg);
            return false;
        }, thisArg), false);

        // Passing `thisArg` has no effect if callback is an arrow function.
        var self = this;
        assertEq(arr.some(() => {
            assertEq(this, self);
            return false;
        }, thisArg), false);
    }
    assertThisArg([1, 2, 3], [1, 2, 3]);
    assertThisArg(Object, Object);
    assertThisArg(1, Object(1));
    assertThisArg("1", Object("1"));
    assertThisArg(false, Object(false));
    assertThisArg(undefined, this);
    assertThisArg(null, this);

    // Throw an exception in the callback.
    var sum = 0;
    var count = 0;
    var thrown = false;
    try {
        arr.some((v, k, o) => {
            count++;
            sum += v;
            assertEq(k, v - 1);
            assertEq(o, arr);
            if (v === 3) {
                throw "some";
            }
            return false
        })
    } catch(e) {
        assertEq(e, "some");
        thrown = true;
    }
    assertEq(thrown, true);
    assertEq(sum, 6);
    assertEq(count, 3);

    // There is no callback or callback is not a function.
    assertThrowsInstanceOf(() => {
        arr.some();
    }, TypeError);
    var invalidCallbacks = [undefined, null, 1, false, "", Symbol(), [], {}, /./];
    invalidCallbacks.forEach(callback => {
        assertThrowsInstanceOf(() => {
            arr.some(callback);
        }, TypeError);
    })

    // Callback is a generator.
    arr.some(function*(){
        throw "This line will not be executed";
    });

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var some = newGlobal()[constructor.name].prototype.some;
        var sum = 0;
        assertEq(some.call(new constructor([1, 2, 3]), v => {
            sum += v;
            return false;
        }), false);
        assertEq(sum, 6);
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.some.call(invalidReceiver, () => true);
        }, TypeError, "Assert that some fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    assertEq(Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).some(() => false), false);
}

for (let constructor of anyTypedArrayConstructors.filter(isFloatConstructor)) {
    assertEq(new constructor([undefined, , NaN]).some(v => v === v), false);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
