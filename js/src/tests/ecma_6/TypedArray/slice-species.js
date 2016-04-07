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
    // Basic tests for our SpeciesConstructor implementation.
    var undefConstructor = new constructor(2);
    undefConstructor.constructor = undefined;
    assertDeepEq(undefConstructor.slice(1), new constructor(1));

    assertThrowsInstanceOf(() => {
        var strConstructor = new constructor;
        strConstructor.constructor = "not a constructor";
        strConstructor.slice(123);
    }, TypeError, "Assert that we have an invalid constructor");

    // If obj.constructor[@@species] is undefined or null then the default
    // constructor is used.
    var mathConstructor = new constructor(8);
    mathConstructor.constructor = Math.sin;
    assertDeepEq(mathConstructor.slice(4), new constructor(4));

    var undefSpecies = new constructor(2);
    undefSpecies.constructor = { [Symbol.species]: undefined };
    assertDeepEq(undefSpecies.slice(1), new constructor(1));

    var nullSpecies = new constructor(2);
    nullSpecies.constructor = { [Symbol.species]: null };
    assertDeepEq(nullSpecies.slice(1), new constructor(1));

    // If obj.constructor[@@species] is different constructor, it should be
    // used.
    for (var constructor2 of constructors) {
        var modifiedConstructor = new constructor(2);
        modifiedConstructor.constructor = constructor2;
        assertDeepEq(modifiedConstructor.slice(1), new constructor2(1));

        var modifiedSpecies = new constructor(2);
        modifiedSpecies.constructor = { [Symbol.species]: constructor2 };
        assertDeepEq(modifiedSpecies.slice(1), new constructor2(1));
    }

    // If obj.constructor[@@species] is neither undefined nor null, and it's
    // not a constructor, TypeError should be thrown.
    assertThrowsInstanceOf(() => {
        var strSpecies = new constructor;
        strSpecies.constructor = { [Symbol.species]: "not a constructor" };
        strSpecies.slice(123);
    }, TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
