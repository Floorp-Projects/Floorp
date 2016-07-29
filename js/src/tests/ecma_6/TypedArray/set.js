const TypedArrayPrototype = Object.getPrototypeOf(Int8Array.prototype);

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

// %TypedArrayPrototype% has an own "set" function property.
assertEq(TypedArrayPrototype.hasOwnProperty("set"), true);
assertEq(typeof TypedArrayPrototype.set, "function");

// The concrete TypedArray prototypes do not have an own "set" property.
assertEq(constructors.every(c => !c.hasOwnProperty("set")), true);

assertDeepEq(Object.getOwnPropertyDescriptor(TypedArrayPrototype, "set"), {
    value: TypedArrayPrototype.set,
    writable: true,
    enumerable: false,
    configurable: true,
});

assertEq(TypedArrayPrototype.set.name, "set");
assertEq(TypedArrayPrototype.set.length, 1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
