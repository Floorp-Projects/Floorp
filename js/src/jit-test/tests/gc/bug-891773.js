x = newGlobal()
Int32Array = x.Int32Array
x.p = ArrayBuffer()
schedulegc(29);
(function(stdlib, n, heap) {
    "use asm"
    var Int32ArrayView = new stdlib.Int32Array(heap)
    function f() {
        Int32ArrayView[1]
    }
    return f
})(this, {
    f: new Function
}, ArrayBuffer())
