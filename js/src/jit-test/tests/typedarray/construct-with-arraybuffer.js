// Test TypedArray constructor when called with ArrayBuffers.

function testArrayBuffer() {
    function test() {
        var ab = new ArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(ab);
            assertEq(ta.length, 4);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testArrayBuffer();

function testArrayBufferAndByteOffset() {
    function test() {
        var ab = new ArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(ab, Int32Array.BYTES_PER_ELEMENT);
            assertEq(ta.length, 3);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testArrayBufferAndByteOffset();

function testArrayBufferAndByteOffsetAndLength() {
    function test() {
        var ab = new ArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(ab, Int32Array.BYTES_PER_ELEMENT, 2);
            assertEq(ta.length, 2);
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
        var ab = new g.ArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT);
        for (var i = 0; i < 1000; ++i) {
            var ta = new Int32Array(ab);
            assertEq(ta.length, 4);
        }
    }
    for (var i = 0; i < 2; ++i) {
        test();
    }
}
testWrappedArrayBuffer();
