// Test tracing of a single linked ArrayBufferViewObject.

function f() {
    var x = new SharedArrayBuffer(0x1000);
    var y = new SharedInt32Array(x);
    gc();
}

if (this.SharedArrayBuffer && this.SharedInt32Array)
    f();
