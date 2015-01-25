const constructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array
];

for (var constructor of constructors) {
    assertEq(constructor.prototype.keys.length, 0);
    assertEq(constructor.prototype.keys.name, "keys");

    assertDeepEq([...new constructor(0).keys()], []);
    assertDeepEq([...new constructor(1).keys()], [0]);
    assertDeepEq([...new constructor(2).keys()], [0, 1]);
    assertDeepEq([...new constructor([15]).keys()], [0]);

    var arr = new constructor([1, 2, 3]);
    var iterator = arr.keys();
    assertDeepEq(iterator.next(), {value: 0, done: false});
    assertDeepEq(iterator.next(), {value: 1, done: false});
    assertDeepEq(iterator.next(), {value: 2, done: false});
    assertDeepEq(iterator.next(), {value: undefined, done: true});

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var keys = newGlobal()[constructor.name].prototype.keys;
        assertDeepEq([...keys.call(new constructor(2))], [0, 1]);
        arr = newGlobal()[constructor.name](2);
        assertEq([...constructor.prototype.keys.call(arr)].toString(), "0,1");
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.keys.call(invalidReceiver);
        }, TypeError, "Assert that keys fails if this value is not a TypedArray");
    });
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
