for (var constructor of anyTypedArrayConstructors) {
    assertDeepEq(constructor.prototype.reverse.length, 0);

    assertDeepEq(new constructor().reverse(), new constructor());
    assertDeepEq(new constructor(10).reverse(), new constructor(10));
    assertDeepEq(new constructor([]).reverse(), new constructor([]));
    assertDeepEq(new constructor([1]).reverse(), new constructor([1]));
    assertDeepEq(new constructor([1, 2]).reverse(), new constructor([2, 1]));
    assertDeepEq(new constructor([1, 2, 3]).reverse(), new constructor([3, 2, 1]));
    assertDeepEq(new constructor([1, 2, 3, 4]).reverse(), new constructor([4, 3, 2, 1]));
    assertDeepEq(new constructor([1, 2, 3, 4, 5]).reverse(), new constructor([5, 4, 3, 2, 1]));
    assertDeepEq(new constructor([.1, .2, .3]).reverse(), new constructor([.3, .2, .1]));

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var reverse = newGlobal()[constructor.name].prototype.reverse;
        assertDeepEq(reverse.call(new constructor([3, 2, 1])), new constructor([1, 2, 3]));
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.reverse.call(invalidReceiver);
        }, TypeError, "Assert that reverse fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).reverse();
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
