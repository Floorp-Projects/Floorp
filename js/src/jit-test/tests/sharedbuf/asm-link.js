// Don't assert.
function $(stdlib, foreign, heap) {
    "use asm";
    var Float64ArrayView = new stdlib.Float64Array(heap);
    function f() {}
    return f
}

if (typeof SharedArrayBuffer !== "undefined")
    $(this, {}, new SharedArrayBuffer(4096));
