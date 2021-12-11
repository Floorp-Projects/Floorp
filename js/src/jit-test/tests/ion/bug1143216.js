// Note: This test produces a link error which is required to reproduce the
// original issue.
m = (function(stdlib, n, heap) {
    "use asm"
    var Float64ArrayView = new stdlib.Float64Array(heap)
    var Int16ArrayView = new stdlib.Int16Array(heap)
    function f(i0) {
        i0 = i0 | 0
        i0 = i0 | 0
        Int16ArrayView[0] = (i0 << 0) + i0
        Float64ArrayView[0]
    }
    return f
})(this, {}, Array)
for (var j = 0; j < 9; j++) {
    m()
}
