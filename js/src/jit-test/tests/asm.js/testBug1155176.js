if (!this.SharedArrayBuffer || !isAsmJSCompilationAvailable())
    quit(0);

load(libdir + "asm.js");
setJitCompilerOption('wasm.test-mode', 1);

// The way this is constructed, either the first module does not
// verify as asm.js (if the >>>0 is left off, which was legal prior to
// bug 1155176), or the results of the two modules have to be equal.

var m = asmCompile("stdlib", "ffi", "heap", `
    "use asm";

    var view = new stdlib.Uint32Array(heap);
    var cas = stdlib.Atomics.compareExchange;
    var hi = ffi.hi;

    function run() {
	hi(+(cas(view, 37, 0, 0)>>>0));
    }

    return run;
`);

assertEq(isAsmJSModule(m), true);

function nonm(stdlib, ffi, heap) {

    var view = new stdlib.Uint32Array(heap);
    var cas = stdlib.Atomics.compareExchange;
    var hi = ffi.hi;

    function run() {
	hi(+cas(view, 37, 0, 0));
    }

    return run;
}

var sab = new SharedArrayBuffer(65536);
var ua = new Uint32Array(sab);
var results = [];
var mrun = m(this, {hi: function (x) { results.push(x) }}, sab);
var nonmrun = nonm(this, {hi: function (x) { results.push(x) }}, sab);

ua[37] = 0x80000001;

mrun();
nonmrun();

assertEq(results[0], ua[37]);
assertEq(results[0], results[1]);
