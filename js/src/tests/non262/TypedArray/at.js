for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.at.length, 1);

    assertEq(new constructor([0]).at(0), 0);
    assertEq(new constructor([0]).at(-1), 0);

    assertEq(new constructor([]).at(0), undefined);
    assertEq(new constructor([]).at(-1), undefined);
    assertEq(new constructor([]).at(1), undefined);

    assertEq(new constructor([0, 1]).at(0), 0);
    assertEq(new constructor([0, 1]).at(1), 1);
    assertEq(new constructor([0, 1]).at(-2), 0);
    assertEq(new constructor([0, 1]).at(-1), 1);

    assertEq(new constructor([0, 1]).at(2), undefined);
    assertEq(new constructor([0, 1]).at(-3), undefined);
    assertEq(new constructor([0, 1]).at(-4), undefined);
    assertEq(new constructor([0, 1]).at(Infinity), undefined);
    assertEq(new constructor([0, 1]).at(-Infinity), undefined);
    assertEq(new constructor([0, 1]).at(NaN), 0); // ToInteger(NaN) = 0

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var at = newGlobal()[constructor.name].prototype.at;
        assertEq(at.call(new constructor([1, 2, 3]), 2), 3);
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.at.call(invalidReceiver);
        }, TypeError, "Assert that 'at' fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    assertEq(Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).at(1), 2);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
