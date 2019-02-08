// |jit-test| skip-if: !this.SharedArrayBuffer

// Test TypedArray constructor when called with SharedArrayBuffers.

function testSharedArrayBuffer() {
    function test() {
        var sab = new SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(sab);
            assertEq(ta.length, 4);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testSharedArrayBuffer();

function testSharedArrayBufferAndByteOffset() {
    function test() {
        var sab = new SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(sab, Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 3);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testSharedArrayBufferAndByteOffset();

function testSharedArrayBufferAndByteOffsetAndLength() {
    function test() {
        var sab = new SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(sab, Int32Array.BYTES_PER_ELEMENT, 2);
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
        var sab = new g.SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(sab);
            assertEq(ta.length, 4);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testWrappedSharedArrayBuffer();
