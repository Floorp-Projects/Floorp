for (var constructor of anyTypedArrayConstructors) {
    assertEq(constructor.prototype.join.length, 1);

    assertEq(new constructor([1, 2, 3]).join(), "1,2,3");
    assertEq(new constructor([1, 2, 3]).join(undefined), "1,2,3");
    assertEq(new constructor([1, 2, 3]).join(null), "1null2null3");
    assertEq(new constructor([1, 2, 3]).join(""), "123");
    assertEq(new constructor([1, 2, 3]).join("+"), "1+2+3");
    assertEq(new constructor([1, 2, 3]).join(.1), "10.120.13");
    assertEq(new constructor([1, 2, 3]).join({toString(){return "foo"}}), "1foo2foo3");
    assertEq(new constructor([1]).join("-"), "1");
    assertEq(new constructor().join(), "");
    assertEq(new constructor().join("*"), "");
    assertEq(new constructor(1).join(), "0");
    assertEq(new constructor(3).join(), "0,0,0");

    assertThrowsInstanceOf(() => new constructor().join({toString(){throw new TypeError}}), TypeError);
    assertThrowsInstanceOf(() => new constructor().join(Symbol()), TypeError);

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var join = newGlobal()[constructor.name].prototype.join;
        assertEq(join.call(new constructor([1, 2, 3]), "\t"), "1\t2\t3");
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.join.call(invalidReceiver);
        }, TypeError, "Assert that join fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    assertEq(Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).join("\0"), "1\0002\0003");
}

for (let constructor of anyTypedArrayConstructors.filter(isFloatConstructor)) {
    assertDeepEq(new constructor([null, , NaN]).join(), "0,NaN,NaN");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
