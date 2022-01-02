// |jit-test| skip-if: !this.SharedArrayBuffer

// Test tracing of two views of a SharedArrayBuffer. Uses a different path.

function f() {
    var x = new SharedArrayBuffer(0x1000);
    var y = new Int32Array(x);
    var z = new Int8Array(x);
    gc();
}

f();
