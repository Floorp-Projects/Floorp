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
    assertThrowsInstanceOf(() => constructor(), TypeError);
    assertThrowsInstanceOf(() => constructor(1), TypeError);
    assertThrowsInstanceOf(() => constructor.call(null), TypeError);
    assertThrowsInstanceOf(() => constructor.apply(null, []), TypeError);
    assertThrowsInstanceOf(() => Reflect.apply(constructor, null, []), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
