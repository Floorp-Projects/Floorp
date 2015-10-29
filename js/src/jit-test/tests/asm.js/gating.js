// Check gating of shared memory features in asm.js (bug 1171540).
//
// When run with -w this should produce a slew of warnings if shared
// memory is not enabled.  There are several cases here because there
// are various checks within Odin.
//
// Note code is not run, so the only issue here is whether it compiles
// properly as asm.js.

if (!this.SharedArrayBuffer || !isAsmJSCompilationAvailable())
    quit(0);

function module_a(stdlib, foreign, heap) {
    "use asm";

    // Without shared memory, this will be flagged as illegal view type
    var view = stdlib.SharedInt32Array;
    var i32a = new view(heap);
    var ld = stdlib.Atomics.load;

    function do_load() {
	var v = 0;
	v = ld(i32a, 0)|0;
	return v|0;
    }

    return { load: do_load };
}

if (this.SharedArrayBuffer)
    assertEq(isAsmJSModule(module_a), true);

function module_b(stdlib, foreign, heap) {
    "use asm";

    // Without shared memory, this will be flagged as illegal view type
    var i32a = new stdlib.SharedInt32Array(heap);
    var ld = stdlib.Atomics.load;

    function do_load() {
	var v = 0;
	v = ld(i32a, 0)|0;
	return v|0;
    }

    return { load: do_load };
}

if (this.SharedArrayBuffer)
    assertEq(isAsmJSModule(module_b), true);

function module_d(stdlib, foreign, heap) {
    "use asm";

    var i32a = new stdlib.Int32Array(heap);
    var ld = stdlib.Atomics.load;

    function do_load() {
	var v = 0;
	// This should be flagged as a type error (needs shared view) regardless
	// of whether shared memory is enabled.
	v = ld(i32a, 0)|0;
	return v|0;
    }

    return { load: do_load };
}

// module_d should never load properly.
