// |jit-test| skip-if: !this.SharedArrayBuffer

// Test tracing of a single linked ArrayBufferViewObject.

function f() {
    var x = new SharedArrayBuffer(0x1000);
    var y = new Int32Array(x);
    gc();
}

f();
