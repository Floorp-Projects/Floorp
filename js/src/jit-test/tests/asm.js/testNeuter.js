load(libdir + "asm.js");

function f(stdlib, foreign, buffer) {
    "use asm";
    var i32 = new stdlib.Int32Array(buffer);
    function set(i,j) {
        i=i|0;
        j=j|0;
        i32[i>>2] = j;
    }
    function get(i) {
        i=i|0;
        return i32[i>>2]|0
    }
    return {get:get, set:set}
}
if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(f), true);

var i32 = new Int32Array(1024);
var buffer = i32.buffer;
var {get, set} = f(this, null, buffer);
if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSFunction(get) && isAsmJSFunction(set), true);

set(4, 42);
assertEq(get(4), 42);

neuter(buffer);

// These operations may throw internal errors
try {
    assertEq(get(4), 0);
    set(0, 42);
    assertEq(get(0), 0);
} catch (e) {
    assertEq(String(e).indexOf("InternalError"), 0);
}

function f2(stdlib, foreign, buffer) {
    "use asm";
    var i32 = new stdlib.Int32Array(buffer);
    var ffi = foreign.ffi;
    function inner(i) {
        i=i|0;
        ffi();
        return i32[i>>2]|0
    }
    return inner
}
if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(f2), true);

var i32 = new Int32Array(1024);
var buffer = i32.buffer;
var threw = false;
function ffi() {
    try {
        neuter(buffer);
    } catch (e) {
        assertEq(String(e).indexOf("InternalError"), 0);
        threw = true;
    }
}
var inner = f2(this, {ffi:ffi}, buffer);
if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSFunction(inner), true);
i32[2] = 13;
var result = inner(8);
if (threw)
    assertEq(result, 13);
else
    assertEq(result, 0);

