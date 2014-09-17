// Test tracing of two views of a SharedArrayBuffer. Uses a different path.

function f() {
    var x = new SharedArrayBuffer(0x1000);
    var y = new SharedInt32Array(x);
    var z = new SharedInt8Array(x);
    gc();
}

if (this.SharedArrayBuffer && this.SharedInt32Array && this.SharedInt8Array)
    f();
