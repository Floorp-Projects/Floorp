// Tests for TypedArray#reduce.
for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.reduce.length, 1);

    // Basic tests.
    var arr = new constructor([1, 2, 3, 4, 5]);

    assertEq(arr.reduce((previous, current) => previous + current), 15);
    assertEq(arr.reduce((previous, current) => current - previous), 3);

    var count = 0;
    var sum = 0;
    assertEq(arr.reduce((previous, current, index, array) => {
        count++;
        sum += current;
        assertEq(current - 1, index);
        assertEq(current, arr[index]);
        assertEq(array, arr);
        return previous * current;
    }), 120);
    assertEq(count, 4);
    assertEq(sum, 14);

    // Tests for `initialValue` argument.
    assertEq(arr.reduce((previous, current) => previous + current, -15), 0);
    assertEq(arr.reduce((previous, current) => previous + current, ""), "12345");
    assertDeepEq(arr.reduce((previous, current) => previous.concat(current), []), [1, 2, 3, 4, 5]);

    // Tests for `this` value.
    var global = this;
    arr.reduce(function(){
        assertEq(this, global);
    });
    arr.reduce(function(){
        "use strict";
        assertEq(this, undefined);
    });
    arr.reduce(() => assertEq(this, global));

    // Throw an exception in the callback.
    var count = 0;
    var sum = 0;
    assertThrowsInstanceOf(() => {
        arr.reduce((previous, current, index, array) => {
            count++;
            sum += current;
            if (index === 3) {
                throw TypeError("reduce");
            }
        })
    }, TypeError);
    assertEq(count, 3);
    assertEq(sum, 9);

    // There is no callback or callback is not a function.
    assertThrowsInstanceOf(() => {
        arr.reduce();
    }, TypeError);
    var invalidCallbacks = [undefined, null, 1, false, "", Symbol(), [], {}, /./];
    invalidCallbacks.forEach(callback => {
        assertThrowsInstanceOf(() => {
            arr.reduce(callback);
        }, TypeError);
    })

    // Callback is a generator.
    arr.reduce(function*(){
        throw "This line will not be executed";
    });

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var reduce = newGlobal()[constructor.name].prototype.reduce;
        assertEq(reduce.call(arr, (previous, current) => Math.min(previous, current)), 1);
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(3), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.reduce.call(invalidReceiver, () => {});
        }, TypeError, "Assert that reduce fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    assertEq(Object.defineProperty(arr, "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).reduce((previous, current) => Math.max(previous, current)), 5);
}

// Tests for TypedArray#reduceRight.
for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.reduceRight.length, 1);

    // Basic tests.
    var arr = new constructor([1, 2, 3, 4, 5]);

    assertEq(arr.reduceRight((previous, current) => previous + current), 15);
    assertEq(arr.reduceRight((previous, current) => current - previous), 3);

    var count = 0;
    var sum = 0;
    assertEq(arr.reduceRight((previous, current, index, array) => {
        count++;
        sum += current;
        assertEq(current - 1, index);
        assertEq(current, arr[index]);
        assertEq(array, arr);
        return previous * current;
    }), 120);
    assertEq(count, 4);
    assertEq(sum, 10);

    // Tests for `initialValue` argument.
    assertEq(arr.reduceRight((previous, current) => previous + current, -15), 0);
    assertEq(arr.reduceRight((previous, current) => previous + current, ""), "54321");
    assertDeepEq(arr.reduceRight((previous, current) => previous.concat(current), []), [5, 4, 3, 2, 1]);

    // Tests for `this` value.
    var global = this;
    arr.reduceRight(function(){
        assertEq(this, global);
    });
    arr.reduceRight(function(){
        "use strict";
        assertEq(this, undefined);
    });
    arr.reduceRight(() => assertEq(this, global));

    // Throw an exception in the callback.
    var count = 0;
    var sum = 0;
    assertThrowsInstanceOf(() => {
        arr.reduceRight((previous, current, index, array) => {
            count++;
            sum += current;
            if (index === 1) {
                throw TypeError("reduceRight");
            }
        })
    }, TypeError);
    assertEq(count, 3);
    assertEq(sum, 9);

    // There is no callback or callback is not a function.
    assertThrowsInstanceOf(() => {
        arr.reduceRight();
    }, TypeError);
    var invalidCallbacks = [undefined, null, 1, false, "", Symbol(), [], {}, /./];
    invalidCallbacks.forEach(callback => {
        assertThrowsInstanceOf(() => {
            arr.reduceRight(callback);
        }, TypeError);
    })

    // Callback is a generator.
    arr.reduceRight(function*(){
        throw "This line will not be executed";
    });

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var reduceRight = newGlobal()[constructor.name].prototype.reduceRight;
        assertEq(reduceRight.call(arr, (previous, current) => Math.min(previous, current)), 1);
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(3), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.reduceRight.call(invalidReceiver, () => {});
        }, TypeError, "Assert that reduceRight fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    assertEq(Object.defineProperty(arr, "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).reduceRight((previous, current) => Math.max(previous, current)), 5);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
