function f(stdlib, foreign, buffer) {
    "use asm";
    var i32 = new stdlib.Int32Array(buffer);
    function main(n) {
        n = n|0;
        var i = 0, sum = 0;
        for (; (i|0) < (n|0); i=(i+1)|0)
            sum = (sum + (i32[(i<<2)>>2]|0))|0;
        return sum|0;
    }
    return main;
}

var i32 = new Int32Array(4096/4);
for (var i = 0; i < 100; i++)
    i32[i] = i;

var fMain = f(this, null, i32.buffer);
if (fMain(4) !== 6)
    throw "f.main(4)";
if (fMain(100) !== 4950)
    throw "f.main(100)";
if (fMain(3000000) !== 4950)
    throw "f.main(3000000)";

postMessage("ok");
