for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.item.length, 1);

    assertEq(new constructor([0]).item(0), 0);
    assertEq(new constructor([0]).item(-1), 0);

    assertEq(new constructor([]).item(0), undefined);
    assertEq(new constructor([]).item(-1), undefined);
    assertEq(new constructor([]).item(1), undefined);

    assertEq(new constructor([0, 1]).item(0), 0);
    assertEq(new constructor([0, 1]).item(1), 1);
    assertEq(new constructor([0, 1]).item(-2), 0);
    assertEq(new constructor([0, 1]).item(-1), 1);

    assertEq(new constructor([0, 1]).item(2), undefined);
    assertEq(new constructor([0, 1]).item(-3), undefined);
    assertEq(new constructor([0, 1]).item(-4), undefined);
    assertEq(new constructor([0, 1]).item(Infinity), undefined);
    assertEq(new constructor([0, 1]).item(-Infinity), undefined);
    assertEq(new constructor([0, 1]).item(NaN), 0); // ToInteger(NaN) = 0

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var item = newGlobal()[constructor.name].prototype.item;
        assertEq(item.call(new constructor([1, 2, 3]), 2), 3);
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.item.call(invalidReceiver);
        }, TypeError, "Assert that item fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    assertEq(Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).item(1), 2);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
