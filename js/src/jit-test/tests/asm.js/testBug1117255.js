function f(stdlib, foreign, buffer) {
    "use asm";
    var i32 = new stdlib.Int32Array(buffer);
    function g(i) {
        i=i|0;
        var j=0;
        for (; (j>>>0) < 100000; j=(j+1)|0)
            i32[i>>2] = j;
    }
    return g
}
var g = f(this, null, new ArrayBuffer(1<<16));
timeout(.1, function cb() { return true });
g(1<<16);
