(function(stdlib, n, heap) {
    "use asm";
    var Uint8ArrayView = new stdlib.Uint8Array(heap);
    function f(d1) {
        d1 = +d1;
        var d2 = .0;
        Uint8ArrayView[d1 < d2] = 0 + 3 + (d2 > -0);
    }
})(this, 0>>0, new Int32Array(0))
