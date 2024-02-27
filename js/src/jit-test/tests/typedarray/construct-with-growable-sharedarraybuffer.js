// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize||!this.SharedArrayBuffer

// Test TypedArray constructor when called with growable SharedArrayBuffers.

function testSharedArrayBuffer() {
    function test() {
        var N = 200;
        var sab = new SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT, {maxByteLength: (5 + N) * Int32Array.BYTES_PER_ELEMENT});
        for (var i = 0; i < N; ++i) {
            var ta = new Int32Array(sab);
            assertEq(ta.length, 4 + i);

            // Ensure auto-length tracking works correctly.
            sab.grow((5 + i) * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 5 + i);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testSharedArrayBuffer();

function testSharedArrayBufferAndByteOffset() {
    function test() {
        var N = 200;
        var sab = new SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT, {maxByteLength: (5 + N) * Int32Array.BYTES_PER_ELEMENT});
        for (var i = 0; i < N; ++i) {
            var ta = new Int32Array(sab, Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 3 + i);

            // Ensure auto-length tracking works correctly.
            sab.grow((5 + i) * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 4 + i);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testSharedArrayBufferAndByteOffset();

function testSharedArrayBufferAndByteOffsetAndLength() {
    function test() {
        var N = 200;
        var sab = new SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT, {maxByteLength: (5 + N) * Int32Array.BYTES_PER_ELEMENT});
        for (var i = 0; i < N; ++i) {
            var ta = new Int32Array(sab, Int32Array.BYTES_PER_ELEMENT, 2);
            assertEq(ta.length, 2);

            // Ensure length doesn't change when resizing the buffer.
            sab.grow((5 + i) * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 2);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testSharedArrayBufferAndByteOffsetAndLength();

function testWrappedSharedArrayBuffer() {
    var g = newGlobal();

    function test() {
        var N = 200;
        var sab = new g.SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT, {maxByteLength: (5 + N) * Int32Array.BYTES_PER_ELEMENT});
        for (var i = 0; i < N; ++i) {
            var ta = new Int32Array(sab);
            assertEq(ta.length, 4 + i);

            // Ensure auto-length tracking works correctly.
            sab.grow((5 + i) * Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 5 + i);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testWrappedSharedArrayBuffer();
