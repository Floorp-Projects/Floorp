// Ensure %TypedArray%.prototype.fill checks for detached buffers.

function DetachArrayBufferValue(buffer, value) {
    return {
        valueOf() {
            detachArrayBuffer(buffer);
            return value;
        }
    };
}

function DetachTypedArrayValue(ta, value) {
    return {
        valueOf() {
            detachArrayBuffer(ta.buffer);
            return value;
        }
    };
}

// Test when ArrayBuffer is already reified.
for (let length of [0, 1, 10, 4096]) {
    let ta = new Int32Array(length);
    let value = DetachArrayBufferValue(ta.buffer, 123);
    assertThrowsInstanceOf(() => ta.fill(value), TypeError);
}

// Test when ArrayBuffer is reified during the fill() call.
for (let length of [0, 1, 10, 4096]) {
    let ta = new Int32Array(length);
    let value = DetachTypedArrayValue(ta, 123);
    assertThrowsInstanceOf(() => ta.fill(value), TypeError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
