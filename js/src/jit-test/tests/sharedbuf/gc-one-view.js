// Test tracing of a single linked ArrayBufferViewObject.

function f() {
    var x = new SharedArrayBuffer(0x1000);
    var y = new Int32Array(x);
    gc();
}

if (typeof SharedArrayBuffer !== "undefined")
    f();
