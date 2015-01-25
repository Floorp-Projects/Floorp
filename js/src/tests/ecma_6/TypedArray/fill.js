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
    assertDeepEq(constructor.prototype.fill.length, 1);

    assertDeepEq(new constructor([]).fill(1), new constructor([]));
    assertDeepEq(new constructor([1,1,1]).fill(2), new constructor([2,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 1), new constructor([1,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 1, 2), new constructor([1,2,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, -2), new constructor([1,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, -2, -1), new constructor([1,2,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, undefined), new constructor([2,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, undefined, undefined), new constructor([2,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 1, undefined), new constructor([1,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, undefined, 1), new constructor([2,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 2, 1), new constructor([1,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, -1, 1), new constructor([1,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, -2, 1), new constructor([1,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 1, -2), new constructor([1,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 0.1), new constructor([2,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 0.9), new constructor([2,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 1.1), new constructor([1,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 0.1, 0.9), new constructor([1,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 0.1, 1.9), new constructor([2,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 0.1, 1.9), new constructor([2,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, -0), new constructor([2,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 0, -0), new constructor([1,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, NaN), new constructor([2,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 0, NaN), new constructor([1,1,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, false), new constructor([2,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, true), new constructor([1,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, "0"), new constructor([2,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, "1"), new constructor([1,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, "-2"), new constructor([1,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, "-2", "-1"), new constructor([1,2,1]));
    assertDeepEq(new constructor([1,1,1]).fill(2, {valueOf: ()=>1}), new constructor([1,2,2]));
    assertDeepEq(new constructor([1,1,1]).fill(2, 0, {valueOf: ()=>1}), new constructor([2,1,1]));

    // Called from other globals.
    if (typeof newGlobal === "function") {
        var fill = newGlobal()[constructor.name].prototype.fill;
        assertDeepEq(fill.call(new constructor([3, 2, 1]), 2), new constructor([2, 2, 2]));
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.fill.call(invalidReceiver, 1);
        }, TypeError);
    });

    // Test that the length getter is never called.
    Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).fill(1);
}

assertDeepEq(new Float32Array([0, 0]).fill(NaN), new Float32Array([NaN, NaN]));
assertDeepEq(new Float64Array([0, 0]).fill(NaN), new Float64Array([NaN, NaN]));

if (typeof reportCompare === "function")
    reportCompare(true, true);
