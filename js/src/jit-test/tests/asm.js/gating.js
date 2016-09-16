// Check gating of shared memory features in asm.js (bug 1171540,
// bug 1231624, bug 1231338, bug 1231335).
//
// In asm.js, importing any atomic is a signal that shared memory is
// being used.  If an atomic is imported, and if shared memory is
// disabled in the build or in the run then an error should be
// signaled for the module.
//
// We check these constraints during linking: the linker checks that
// the buffer has the right type and that the Atomics - if used - have
// their expected values; if shared memory is disabled then the
// Atomics object will be absent from the global or it will have
// values that are not the expected built-in values and the link will
// fail as desired.

load(libdir + "asm.js");

if (!isAsmJSCompilationAvailable())
    quit(0);

setJitCompilerOption('asmjs.atomics.enable', 1);

if (!this.Atomics) {
    this.Atomics = { load: function (x, y) { return 0 },
		     store: function (x, y, z) { return 0 },
		     exchange: function (x, y, z) { return 0 },
		     add: function (x, y, z) { return 0 },
		     sub: function (x, y, z) { return 0 },
		     and: function (x, y, z) { return 0 },
		     or: function (x, y, z) { return 0 },
		     xor: function (x, y, z) { return 0 },
		     compareExchange: function (x, y, z, w) { return 0 }
		   };
}


var module_a = asmCompile("stdlib", "foreign", "heap", `
    "use asm";

    var ld = stdlib.Atomics.load;

    function f() { return 0; }
    return { f:f };
`);

var module_b = asmCompile("stdlib", "foreign", "heap", `
    "use asm";

    var ld = stdlib.Atomics.load;
    var i32a = new stdlib.Int32Array(heap);

    function f() { return 0; }
    return { f:f };
`);

var module_c = asmCompile("stdlib", "foreign", "heap", `
    "use asm";

    var i32a = new stdlib.Int32Array(heap);

    function f() { return 0; }
    return { f:f };
`);

assertEq(isAsmJSModule(module_a), true);
assertEq(isAsmJSModule(module_b), true);
assertEq(isAsmJSModule(module_c), true);

if (!this.SharedArrayBuffer) {
    assertAsmLinkFail(module_a, this, {}, new ArrayBuffer(65536));  // Buffer is ignored, Atomics are bad
} else {
    asmLink(module_a, this, {}, new ArrayBuffer(65536));            // Buffer is ignored, Atomics are good
    assertAsmLinkFail(module_b, this, {}, new ArrayBuffer(65536));  // Buffer is wrong type
}

asmLink(module_c, this, {}, new ArrayBuffer(65536));                // Buffer is right type

if (this.SharedArrayBuffer)
    assertAsmLinkFail(module_c, this, {}, new SharedArrayBuffer(65536));  // Buffer is wrong type
