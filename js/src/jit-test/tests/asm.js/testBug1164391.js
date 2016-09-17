if (!this.SharedArrayBuffer)
    quit(0);

load(libdir + "asm.js");
load(libdir + "asserts.js");
setJitCompilerOption('asmjs.atomics.enable', 1);

var m = asmCompile("stdlib", "ffi", "heap", `
    "use asm";
    var HEAP32 = new stdlib.Int32Array(heap);
    var add = stdlib.Atomics.add;
    var load = stdlib.Atomics.load;
    function add_sharedEv(i1) {
        i1 = i1 | 0;
        load(HEAP32, i1 >> 2);
        add(HEAP32, i1 >> 2, 1);
        load(HEAP32, i1 >> 2);
    }
    return {add_sharedEv:add_sharedEv};
`);

if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(m), true);

var sab = new SharedArrayBuffer(65536);
var {add_sharedEv} = m(this, {}, sab);
assertErrorMessage(() => add_sharedEv(sab.byteLength), RangeError, /out-of-range index/);
