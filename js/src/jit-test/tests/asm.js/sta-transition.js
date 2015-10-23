// Transitional test cases, useful while Odin accepts both
// "Int32Array" and "SharedInt32Array" to construct a view onto shared
// memory but the former only when an atomic operation is referenced,
// as per spec.  Eventually it will stop accepting "SharedInt32Array",
// because that name is going away.
//
// These should not run with --no-asmjs.

if (!isAsmJSCompilationAvailable())
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
// SharedInt8Array can still be used on SharedArrayBuffer.
// (SharedInt8Array will eventually disappear, and this
// test case with it.)

function m2(stdlib, ffi, heap) {
    "use asm";

    var i8 = new stdlib.SharedInt8Array(heap);
    var add = stdlib.Atomics.add;

    function g() {
	add(i8, 0, 1);
	return 42;
    }

    return { g:g }
}

assertEq(isAsmJSModule(m2), true);

var { g } = m2(this, {}, new SharedArrayBuffer(65536));
assertEq(g(), 42);

//////////////////////////////////////////////////////////////////////
//
// SharedInt8Array still cannot be used on ArrayBuffer, even without
// atomics present.
// (SharedInt8Array will eventually disappear, and this
// test case with it.)

function m3(stdlib, ffi, heap) {
    "use asm";

    var i8 = new stdlib.SharedInt8Array(heap);

    function h() {
	return i8[0]|0;
    }

    return { h:h }
}

// Running the shell with -w you should see an error here.

assertEq(isAsmJSModule(m3), true);
try {
    var wasThrown = false;
    m3(this, {}, new ArrayBuffer(65536));
}
catch (e) {
    wasThrown = true;
}
assertEq(wasThrown, true);

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
