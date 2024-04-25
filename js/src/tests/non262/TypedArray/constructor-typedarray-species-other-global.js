// 22.2.4.3 TypedArray ( typedArray )

// Test [[Prototype]] of newly created typed array and its array buffer, and
// ensure they are both created in the correct global.

const thisGlobal = this;
const otherGlobal = newGlobal();

const typedArrays = [otherGlobal.eval("new Int32Array(0)")];

if (this.SharedArrayBuffer) {
    typedArrays.push(otherGlobal.eval("new Int32Array(new SharedArrayBuffer(0))"));
}

for (let typedArray of typedArrays) {
    // Ensure the "constructor" property isn't accessed.
    Object.defineProperty(typedArray.buffer, "constructor", {
        get() {
            throw new Error("constructor property accessed");
        }
    });

    for (let ctor of typedArrayConstructors) {
        let newTypedArray = new ctor(typedArray);

        assertEq(Object.getPrototypeOf(newTypedArray), ctor.prototype);
        assertEq(Object.getPrototypeOf(newTypedArray.buffer), ArrayBuffer.prototype);
        assertEq(newTypedArray.buffer.constructor, ArrayBuffer);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
