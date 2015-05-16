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
    assertEq(constructor.of.length, 0);

    assertDeepEq(Object.getOwnPropertyDescriptor(constructor.__proto__, "of"), {
        value: constructor.of,
        writable: true,
        enumerable: false,
        configurable: true
    });

    // Basic tests.
    assertEq(constructor.of().constructor, constructor);
    assertEq(constructor.of() instanceof constructor, true);
    assertDeepEq(constructor.of(10), new constructor([10]));
    assertDeepEq(constructor.of(1, 2, 3), new constructor([1, 2, 3]));
    assertDeepEq(constructor.of("1", "2", "3"), new constructor([1, 2, 3]));

    // This method can be transplanted to other constructors.
    assertDeepEq(constructor.of.call(Array, 1, 2, 3), [1, 2, 3]);

    var hits = 0;
    assertDeepEq(constructor.of.call(function(len) {
        assertEq(arguments.length, 1);
        assertEq(len, 3);
        hits++;
        return {};
    }, "a", "b", "c"), {
        0: "a",
        1: "b",
        2: "c"
    });
    assertEq(hits, 1);

    // Behavior across compartments.
    if (typeof newGlobal === "function") {
        var newC = newGlobal()[constructor.name];
        assertEq(newC.of() instanceof newC, true);
        assertEq(newC.of() instanceof constructor, false);
        assertEq(newC.of.call(constructor) instanceof constructor, true);
    }

    // Throws if `this` isn't a constructor.
    var invalidConstructors = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                               constructor.of, () => {}];
    invalidConstructors.forEach(C => {
        assertThrowsInstanceOf(() => {
            constructor.of.call(C);
        }, TypeError);
    });

    // Throw if `this` is a method definition or a getter/setter function.
    assertThrowsInstanceOf(() => {
        constructor.of.call({method() {}}.method);
    }, TypeError);
    assertThrowsInstanceOf(() => {
        constructor.of.call(Object.getOwnPropertyDescriptor({get getter() {}}, "getter").get);
    }, TypeError);

    // Generators are also legal constructors.
    assertEq(constructor.of.call(function*(len) {
        return len;
    }, "a", "b", "c").next().value, 3);

    // An exception might be thrown in a strict assignment to the new object's indexed properties.
    assertThrowsInstanceOf(() => {
        constructor.of.call(function() {
            return {get 0() {}};
        }, "a");
    }, TypeError);

    assertThrowsInstanceOf(() => {
        constructor.of.call(function() {
            return Object("1");
        }, "a");
    }, TypeError);

    assertThrowsInstanceOf(() => {
        constructor.of.call(function() {
            return Object.create({
                set 0(v) {
                    throw new TypeError;
                }
            });
        }, "a");
    }, TypeError);
}

assertDeepEq(Float32Array.of(0.1, null, undefined, NaN), new Float32Array([0.1, 0, NaN, NaN]));
assertDeepEq(Float64Array.of(0.1, null, undefined, NaN), new Float64Array([0.1, 0, NaN, NaN]));

if (typeof reportCompare === "function")
    reportCompare(true, true);
