load(libdir + "asm.js");

setJitCompilerOption("ion.warmup.trigger", 0);
setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("offthread-compilation.enable", 0);

function ffi1() { assertJitStackInvariants() }
function ffi2() { return { valueOf() { assertJitStackInvariants() } } }

// FFI with no coercion
var m = asmCompile('stdlib', 'foreign', `
    "use asm";
    var ffi = foreign.ffi;
    function f() { ffi(); }
    return f
`);
var f = asmLink(m, null, {ffi:ffi1});
f();
f();

// FFI with ToInt32 coercion
var m = asmCompile('stdlib', 'foreign', `
    "use asm";
    var ffi = foreign.ffi;
    function f() { ffi() | 0; }
    return f
`);
var f = asmLink(m, null, {ffi:ffi1});
f();
f();
var f = asmLink(m, null, {ffi:ffi2});
f();
f();

// FFI with ToNumber coercion
var m = asmCompile('stdlib', 'foreign', `
    "use asm";
    var ffi = foreign.ffi;
    function f() { +ffi(); }
    return f
`);
var f = asmLink(m, this, {ffi:ffi1});
f();
f();
var f = asmLink(m, this, {ffi:ffi2});
f();
f();
