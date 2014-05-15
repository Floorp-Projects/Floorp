// |jit-test| error:TypeError
// stdlib is undefined

(function(stdlib, n, heap) {
    "use asm"
    var Int16ArrayView = new stdlib.Int16Array(heap)
    function f() {
        var x = 4.
        Int16ArrayView[~~((1 ? .0 : .9) % x) >> 1] = 0
        u(x)
    }
})()
