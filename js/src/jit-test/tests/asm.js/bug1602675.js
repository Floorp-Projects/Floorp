// |jit-test| skip-if: !isAsmJSCompilationAvailable()
//
// Confusion between TA constructor and TA view led to invalid wasm code
// generation and a subsequent failure in the wasm verifier.  This code should
// not validate; we should fall back to JS execution.

function f(stdlib, foreign, buffer) {
    "use asm";
    var i32 =stdlib.Int32Array
    function g(i) {
        i=i|0;
        var j=0;
        for (; (j>>>0) < 100000; j=(j+1)|0)
            i32[i>>2] = j;
    }
    return g
}
var g = f(this, null, new ArrayBuffer(1<<16));
g(1<<16);
