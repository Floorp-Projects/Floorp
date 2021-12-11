x = {};
x.toString = (function(stdlib, heap) {
    Int8ArrayView = new stdlib.Int8Array(heap);
    Float32ArrayView = new stdlib.Float32Array(heap);
    function f() {
        Int8ArrayView[0] = Float32ArrayView[0]
    }
    return f
})(this, ArrayBuffer);
x + 1
