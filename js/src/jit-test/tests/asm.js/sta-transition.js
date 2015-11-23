// Test the inference of shared memory in asm.js.
//
// These should not be run with --no-asmjs, the guard below checks this.

if (!this.SharedArrayBuffer || !isAsmJSCompilationAvailable())
    quit(0);

//////////////////////////////////////////////////////////////////////
//
// Int8Array can be used on SharedArrayBuffer, if atomics are present

function m1(stdlib, ffi, heap) {
    "use asm";

    var i8 = new stdlib.Int8Array(heap);
    var add = stdlib.Atomics.add;

    function f() {
	add(i8, 0, 1);
	return 37;
    }

    return { f:f }
}

assertEq(isAsmJSModule(m1), true);

var { f } = m1(this, {}, new SharedArrayBuffer(65536));
assertEq(f(), 37);

//////////////////////////////////////////////////////////////////////
//
// Int8Array cannot be used on SharedArrayBuffer if atomics are not imported.
// One argument for the restriction is that there are some optimizations
// that are legal if the memory is known not to be shared that are illegal
// when it is shared.

function m4(stdlib, ffi, heap) {
    "use asm";

    var i8 = new stdlib.Int8Array(heap);

    function i() {
	return i8[0]|0;
    }

    return { i:i }
}

assertEq(isAsmJSModule(m4), true);

// An error is not actually thrown because the link failure drops us
// back to JS execution and then the Int8Array constructor will copy data
// from the SharedArrayBuffer.
//
// Running the shell with -w you should see an error here.

var { i } = m4(this, {}, new SharedArrayBuffer(65536));
assertEq(isAsmJSFunction(i), false);
