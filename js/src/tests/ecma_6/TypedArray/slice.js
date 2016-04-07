const constructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array ];

if (typeof SharedArrayBuffer != "undefined")
    constructors.push(sharedConstructor(Int8Array),
		      sharedConstructor(Uint8Array),
		      sharedConstructor(Int16Array),
		      sharedConstructor(Uint16Array),
		      sharedConstructor(Int32Array),
		      sharedConstructor(Uint32Array),
		      sharedConstructor(Float32Array),
		      sharedConstructor(Float64Array));

for (var constructor of constructors) {
    assertDeepEq(constructor.prototype.slice.length, 2);

    assertDeepEq(new constructor().slice(0), new constructor());
    assertDeepEq(new constructor().slice(0, 4), new constructor());
    assertDeepEq(new constructor(10).slice(0, 2), new constructor(2));

    assertDeepEq(new constructor([1, 2]).slice(1), new constructor([2]));
    assertDeepEq(new constructor([1, 2]).slice(0), new constructor([1, 2]));
    assertDeepEq(new constructor([1, 2, 3]).slice(-1), new constructor([3]));
    assertDeepEq(new constructor([1, 2, 3, 4]).slice(-3, -1), new constructor([2, 3]));
    assertDeepEq(new constructor([.1, .2]).slice(0), new constructor([.1, .2]));

    assertDeepEq(new constructor([1, 2]).slice(-3), new constructor([1, 2]));
    assertDeepEq(new constructor([1, 2]).slice(0, -3), new constructor());
    assertDeepEq(new constructor([1, 2]).slice(4), new constructor());
    assertDeepEq(new constructor([1, 2]).slice(1, 5), new constructor([2]));

    // Called from other globals.
    if (typeof newGlobal === "function" && !isSharedConstructor(constructor)) {
        var slice = newGlobal()[constructor.name].prototype.slice;
        assertDeepEq(slice.call(new constructor([3, 2, 1]), 1),
                     new constructor([2, 1]));
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.slice.call(invalidReceiver, 0);
        }, TypeError, "Assert that slice fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).slice(2);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);

