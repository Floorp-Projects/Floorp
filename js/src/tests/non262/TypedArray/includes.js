for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.includes.length, 1);

    assertEq(new constructor([1, 2, 3]).includes(1), true);
    assertEq(new constructor([1, 2, 3]).includes(2), true);
    assertEq(new constructor([1, 2, 3]).includes(3), true);
    assertEq(new constructor([1, 2, 3]).includes(2, 1), true);
    assertEq(new constructor([1, 2, 3]).includes(2, -2), true);
    assertEq(new constructor([1, 2, 3]).includes(2, -100), true);

    assertEq(new constructor([1, 2, 3]).includes("2"), false);
    assertEq(new constructor([1, 2, 3]).includes(2, 2), false);
    assertEq(new constructor([1, 2, 3]).includes(2, -1), false);
    assertEq(new constructor([1, 2, 3]).includes(2, 100), false);

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var includes = newGlobal()[constructor.name].prototype.includes;
        assertEq(includes.call(new constructor([1, 2, 3]), 2), true);
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.includes.call(invalidReceiver);
        }, TypeError, "Assert that reverse fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    assertEq(Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).includes(2), true);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
