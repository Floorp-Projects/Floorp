for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.entries.length, 0);
    assertEq(constructor.prototype.entries.name, "entries");

    assertDeepEq([...new constructor(0).entries()], []);
    assertDeepEq([...new constructor(1).entries()], [[0, 0]]);
    assertDeepEq([...new constructor(2).entries()], [[0, 0], [1, 0]]);
    assertDeepEq([...new constructor([15]).entries()], [[0, 15]]);

    var arr = new constructor([1, 2, 3]);
    var iterator = arr.entries();
    assertDeepEq(iterator.next(), {value: [0, 1], done: false});
    assertDeepEq(iterator.next(), {value: [1, 2], done: false});
    assertDeepEq(iterator.next(), {value: [2, 3], done: false});
    assertDeepEq(iterator.next(), {value: undefined, done: true});

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var otherGlobal = newGlobal();
        var entries = otherGlobal[constructor.name].prototype.entries;
        assertDeepEq([...entries.call(new constructor(2))],
                     [new otherGlobal.Array(0, 0), new otherGlobal.Array(1, 0)]);
        arr = new (newGlobal()[constructor.name])(2);
        assertEq([...constructor.prototype.entries.call(arr)].toString(), "0,0,1,0");
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.entries.call(invalidReceiver);
        }, TypeError, "Assert that entries fails if this value is not a TypedArray");
    });
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
