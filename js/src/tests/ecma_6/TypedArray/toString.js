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
    Float64Array,
];

// %TypedArrayPrototype% has an own "toString" property.
assertEq(TypedArrayPrototype.hasOwnProperty("toString"), true);

// The initial value of %TypedArrayPrototype%.toString is Array.prototype.toString.
assertEq(TypedArrayPrototype.toString, Array.prototype.toString);

// The concrete TypedArray prototypes do not have an own "toString" property.
assertEq(constructors.every(c => !c.hasOwnProperty("toString")), true);

assertDeepEq(Object.getOwnPropertyDescriptor(TypedArrayPrototype, "toString"), {
    value: TypedArrayPrototype.toString,
    writable: true,
    enumerable: false,
    configurable: true,
});

for (let constructor of constructors) {
    assertEq(new constructor([]).toString(), "");
    assertEq(new constructor([1]).toString(), "1");
    assertEq(new constructor([1, 2]).toString(), "1,2");
}

assertEq(new Int8Array([-1, 2, -3, 4, NaN]).toString(), "-1,2,-3,4,0");
assertEq(new Uint8Array([255, 2, 3, 4, NaN]).toString(), "255,2,3,4,0");
assertEq(new Uint8ClampedArray([255, 256, 2, 3, 4, NaN]).toString(), "255,255,2,3,4,0");
assertEq(new Int16Array([-1, 2, -3, 4, NaN]).toString(), "-1,2,-3,4,0");
assertEq(new Uint16Array([-1, 2, 3, 4, NaN]).toString(), "65535,2,3,4,0");
assertEq(new Int32Array([-1, 2, -3, 4, NaN]).toString(), "-1,2,-3,4,0");
assertEq(new Uint32Array([-1, 2, 3, 4, NaN]).toString(), "4294967295,2,3,4,0");
assertEq(new Float32Array([-0, 0, 0.5, -0.5, NaN, Infinity, -Infinity]).toString(), "0,0,0.5,-0.5,NaN,Infinity,-Infinity");
assertEq(new Float64Array([-0, 0, 0.5, -0.5, NaN, Infinity, -Infinity]).toString(), "0,0,0.5,-0.5,NaN,Infinity,-Infinity");

if (typeof reportCompare === "function")
    reportCompare(true, true);
