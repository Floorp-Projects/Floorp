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

    // Basic tests for our SpeciesConstructor implementation.
    var undefConstructor = new constructor(2);
    undefConstructor.constructor = undefined;
    assertDeepEq(undefConstructor.slice(1), new constructor(1));

    assertThrowsInstanceOf(() => {
        var strConstructor = new constructor;
        strConstructor.constructor = "not a constructor";
        strConstructor.slice(123);
    }, TypeError, "Assert that we have an invalid constructor");

    // If obj.constructor[@@species] is undefined or null -- which it has to be
    // if we don't implement @@species -- then the default constructor is used.
    var mathConstructor = new constructor(8);
    mathConstructor.constructor = Math.sin;
    assertDeepEq(mathConstructor.slice(4), new constructor(4));

    assertEq(Symbol.species in Int8Array, false,
             "you've implemented %TypedArray%[@@species] -- add real tests here!");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);

