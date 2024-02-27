// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

// Test TypedArray constructor when called with resizable ArrayBuffers.

function testArrayBuffer() {
    function test() {
        var ab = new ArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT, {maxByteLength: 5 * Int32Array.BYTES_PER_ELEMENT});
        for (var i = 0; i < 200; ++i) {
            var ta = new Int32Array(ab);
            assertEq(ta.length, 4);

            // Ensure auto-length tracking works correctly.
            ab.resize(5 * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 5);

            ab.resize(2 * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 2);

            // Reset to original length.
            ab.resize(4 * Int32Array.BYTES_PER_ELEMENT);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testArrayBuffer();

function testArrayBufferAndByteOffset() {
    function test() {
        var ab = new ArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT, {maxByteLength: 5 * Int32Array.BYTES_PER_ELEMENT});
        for (var i = 0; i < 200; ++i) {
            var ta = new Int32Array(ab, Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 3);

            // Ensure auto-length tracking works correctly.
            ab.resize(5 * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 4);

            ab.resize(2 * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 1);

            // Reset to original length.
            ab.resize(4 * Int32Array.BYTES_PER_ELEMENT);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testArrayBufferAndByteOffset();

function testArrayBufferAndByteOffsetAndLength() {
    function test() {
        var ab = new ArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT, {maxByteLength: 5 * Int32Array.BYTES_PER_ELEMENT});
        for (var i = 0; i < 200; ++i) {
            var ta = new Int32Array(ab, Int32Array.BYTES_PER_ELEMENT, 2);
            assertEq(ta.length, 2);

            // Ensure length doesn't change when resizing the buffer.
            ab.resize(5 * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 2);

            // Returns zero when the TypedArray get out-of-bounds.
            ab.resize(2 * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 0);

            // Reset to original length.
            ab.resize(4 * Int32Array.BYTES_PER_ELEMENT);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testArrayBufferAndByteOffsetAndLength();

function testWrappedArrayBuffer() {
    var g = newGlobal();

    function test() {
        var ab = new g.ArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT, {maxByteLength: 5 * Int32Array.BYTES_PER_ELEMENT});
        for (var i = 0; i < 200; ++i) {
            var ta = new Int32Array(ab);
            assertEq(ta.length, 4);

            // Ensure auto-length tracking works correctly.
            ab.resize(5 * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 5);

            ab.resize(2 * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 2);

            // Reset to original length.
            ab.resize(4 * Int32Array.BYTES_PER_ELEMENT);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testWrappedArrayBuffer();
