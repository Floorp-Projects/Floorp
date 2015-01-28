// The bug was that the asm.js linker did not catch that an unshared
// view was created on a shared buffer.  (Nor did it catch the vice
// versa case.)  As a consequence the code was not rejected (and run
// as plain JS) as it should be.  That gave rise to a difference in
// output.

if (!this.SharedArrayBuffer)
    quit(0);

// Original test

g = (function(stdlib, n, heap) {
    "use asm";
    var Float32ArrayView = new stdlib.Float32Array(heap);
    function f() {
        return +Float32ArrayView[0]
    }
    return f
})(this, {}, new SharedArrayBuffer(4096));
assertEq(g(), NaN);

// Additional test: vice versa case.

try {
    g = (function(stdlib, n, heap) {
	"use asm";
	var Float32ArrayView = new stdlib.SharedFloat32Array(heap);
	function f() {
            return +Float32ArrayView[0]
	}
	return f
    })(this, {}, new ArrayBuffer(4096));
    // If the code runs, as it would with the bug present, it must return NaN.
    assertEq(g(), NaN);
}
catch (e) {
    // If the code throws then that's OK too.  The current (January 2015)
    // semantics is for the SharedFloat32Array constructor to throw if
    // handed an ArrayBuffer, but asm.js does not execute that constructor
    // and before the fix it would not throw.
}
