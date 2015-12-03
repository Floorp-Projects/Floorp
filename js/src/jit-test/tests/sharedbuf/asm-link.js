// Don't assert on linking.
// Provide superficial functionality.

function $(stdlib, foreign, heap) {
    "use asm";
    var f64 = new stdlib.Float64Array(heap);
    function f() { var v=0.0; v=+f64[0]; return +v; }
    return f
}

if (this.SharedArrayBuffer) {
    var heap = new SharedArrayBuffer(65536);
    (new Float64Array(heap))[0] = 3.14159;
    assertEq($(this, {}, heap)(), 3.14159);
}
