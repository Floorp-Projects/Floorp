function f1() { "use asm"; function g() {} return g }
if (this.jsFuns) {
    ok(jsFuns.isAsmJSModule(f1), "f1 is an asm.js module");
    ok(jsFuns.isAsmJSFunction(f1()), "f1.g is an asm.js function");
}

function f2(stdlib, foreign, buffer) {
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
if (this.jsFuns)
    ok(jsFuns.isAsmJSModule(f2), "f2 is an asm.js module");
var i32 = new Int32Array(16384); // Smallest allowed buffer size is 64KBy
for (var i = 0; i < i32.length; i++)
    i32[i] = i;
var f2Main = f2(this, null, i32.buffer);
if (this.jsFuns)
    ok(jsFuns.isAsmJSFunction(f2Main), "f2.main is an asm.js function");
if (f2Main(4) !== 6)
    throw "f2Main(4)";
if (f2Main(100) !== 4950)
    throw "f2.main(100)";
var sum = (((i32.length - 1) * i32.length) / 2);
if (f2Main(i32.length) !== sum)
    throw "f2.main(" + i32.length + ")";
if (f2Main(i32.length + 100) !== sum)
    throw "f2.main(" + i32.length + ")";

function f3(stdlib, foreign, buffer) {
    "use asm";
    var done = foreign.done;
    var i32 = new stdlib.Int32Array(buffer);
    function main() {
        var i = 0, sum = 0;
        while (1) {
            for (i = 0; (i|0) < 1000; i=(i+1)|0)
                sum = (sum + i)|0;
            if (done(sum|0)|0)
                break;
        }
        return sum|0;
    }
    return main;
}
var begin;
var lastSum;
function done(sum) {
    if (sum !== ((lastSum + 499500)|0))
        throw "bad sum: " + sum + ", " + lastSum + ", " + ((lastSum + 499500)|0);
    lastSum = sum;
    return (Date.now() - begin) > 3000;
}
var f3Main = f3(this, {done:done}, i32.buffer);
if (this.jsFuns)
    ok(jsFuns.isAsmJSFunction(f3Main), "f3.main is an asm.js function");

begin = Date.now();
lastSum = 0;
if (f3Main() !== lastSum)
    throw "f3.main()";

if (!this.jsFuns)
    postMessage("ok");
else
    complete();
