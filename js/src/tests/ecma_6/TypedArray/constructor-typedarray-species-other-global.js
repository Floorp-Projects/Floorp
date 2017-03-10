// 22.2.4.3 TypedArray ( typedArray )

// Test [[Prototype]] of newly created typed array and its array buffer, and
// ensure they are both created in the correct global.

const thisGlobal = this;
const otherGlobal = newGlobal();
const ta_i32 = otherGlobal.eval("new Int32Array(0)");

function assertBufferPrototypeFrom(newTypedArray, prototype) {
    var typedArrayName = newTypedArray.constructor.name;

    assertEq(Object.getPrototypeOf(newTypedArray), thisGlobal[typedArrayName].prototype);
    assertEq(Object.getPrototypeOf(newTypedArray.buffer), prototype);
}

const EMPTY = {};

// Test SpeciesConstructor() implementation selects the correct (fallback) constructor.
const testCases = [
    // Create the array buffer from the species constructor.
    { constructor: EMPTY, prototype: otherGlobal.ArrayBuffer.prototype },

    // Use %ArrayBuffer% from this global if constructor is undefined.
    { constructor: undefined, prototype: ArrayBuffer.prototype },

    // Use %ArrayBuffer% from this global if species is undefined.
    { constructor: {[Symbol.species]: undefined}, prototype: ArrayBuffer.prototype },

    // Use %ArrayBuffer% from this global if species is null.
    { constructor: {[Symbol.species]: null}, prototype: ArrayBuffer.prototype },
];

for (let { constructor, prototype } of testCases) {
    if (constructor !== EMPTY) {
        ta_i32.buffer.constructor = constructor;
    }

    // Same element type.
    assertBufferPrototypeFrom(new Int32Array(ta_i32), prototype);

    // Different element type.
    assertBufferPrototypeFrom(new Int16Array(ta_i32), prototype);
}


// Also ensure TypeErrors are thrown from the correct global.
const errorTestCases = [
    // Constructor property is neither undefined nor an object.
    { constructor: null },
    { constructor: 123 },

    // Species property is neither undefined/null nor a constructor function.
    { constructor: { [Symbol.species]: 123 } },
    { constructor: { [Symbol.species]: [] } },
    { constructor: { [Symbol.species]: () => {} } },
];

for (let { constructor } of errorTestCases) {
    ta_i32.buffer.constructor = constructor;

    // Same element type.
    assertThrowsInstanceOf(() => new Int32Array(ta_i32), TypeError);

    // Different element type.
    assertThrowsInstanceOf(() => new Int32Array(ta_i32), TypeError);
}


// TypedArrays using SharedArrayBuffers never call the SpeciesConstructor operation.
if (this.SharedArrayBuffer) {
    const ta_i32_shared = otherGlobal.eval("new Int32Array(new SharedArrayBuffer(0))");

    Object.defineProperty(ta_i32_shared.buffer, "constructor", {
        get() {
            throw new Error("constructor property accessed");
        }
    });

    // Same element type.
    assertBufferPrototypeFrom(new Int32Array(ta_i32_shared), ArrayBuffer.prototype);

    // Different element type.
    assertBufferPrototypeFrom(new Int16Array(ta_i32_shared), ArrayBuffer.prototype);
}


if (typeof reportCompare === "function")
    reportCompare(0, 0);
