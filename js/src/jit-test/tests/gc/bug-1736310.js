gczeal(9, 10);
function a() {
    var b = new Int32Array(buffer);
    function c(d) {
        b[5] = d;
    }
    return c;
}
b = new Int32Array(6);
var buffer = b.buffer;
a()({
    valueOf() {
        detachArrayBuffer(buffer);
    }
})

