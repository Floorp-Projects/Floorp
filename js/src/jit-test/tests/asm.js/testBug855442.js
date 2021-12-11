gczeal(2,4);
function asmModule(g, foreign, heap) {
    "use asm";
    var HEAP8 = new g.Int8Array(heap);
    function f() {}
    return {f: f};
}
asmModule(this, 2, new ArrayBuffer(2048));
