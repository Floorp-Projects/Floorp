for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.values.length, 0);
    assertEq(constructor.prototype.values.name, "values");
    assertEq(constructor.prototype.values, constructor.prototype[Symbol.iterator]);

    assertDeepEq([...new constructor(0).values()], []);
    assertDeepEq([...new constructor(1).values()], [0]);
    assertDeepEq([...new constructor(2).values()], [0, 0]);
    assertDeepEq([...new constructor([15]).values()], [15]);

    var arr = new constructor([1, 2, 3]);
    var iterator = arr.values();
    assertDeepEq(iterator.next(), {value: 1, done: false});
    assertDeepEq(iterator.next(), {value: 2, done: false});
    assertDeepEq(iterator.next(), {value: 3, done: false});
    assertDeepEq(iterator.next(), {value: undefined, done: true});

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var values = newGlobal()[constructor.name].prototype.values;
        assertDeepEq([...values.call(new constructor([42, 36]))], [42, 36]);
        arr = new (newGlobal()[constructor.name])([42, 36]);
        assertEq([...constructor.prototype.values.call(arr)].toString(), "42,36");
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.values.call(invalidReceiver);
        }, TypeError, "Assert that values fails if this value is not a TypedArray");
    });
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
