// Check gating of shared memory features in asm.js (bug 1171540,
// bug 1231624).
//
// In asm.js, importing any atomic is a signal that shared memory is
// being used.  If an atomic is imported, and if shared memory is
// disabled in the build or in the run then a type error should be
// signaled for the module at the end of the declaration section and
// the module should not be an asm.js module.

if (!this.SharedArrayBuffer || !isAsmJSCompilationAvailable())
    quit(0);

// This code is not run, we only care whether it compiles as asm.js.

function module_a(stdlib, foreign, heap) {
    "use asm";

    var i32a = new stdlib.Int32Array(heap);
    var ld = stdlib.Atomics.load;

    // There should be a type error around this line if shared memory
    // is not enabled.

    function do_load() {
	var v = 0;
	v = ld(i32a, 0)|0;	// It's not actually necessary to use the atomic op
	return v|0;
    }

    return { load: do_load };
}

assertEq(isAsmJSModule(module_a), !!this.SharedArrayBuffer);
