// |jit-test| error: TypeError

new(function(stdlib, n, heap) {
    "use asm"
    var x = new stdlib.Uint32Array(heap)
    function f() {}
    return f
})
