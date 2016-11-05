load(libdir + "asm.js");
load(libdir + "asserts.js");

if (!isAsmJSCompilationAvailable())
    quit();

var code = USE_ASM + "var i32 = new stdlib.Int32Array(buf); function f() { return i32[0]|0 } return f";

var ab = new ArrayBuffer(BUF_MIN);
new Int32Array(ab)[0] = 42;

var f = asmLink(asmCompile('stdlib', 'ffi', 'buf', code), this, null, ab);
assertEq(f(), 42);

assertThrowsInstanceOf(() => detachArrayBuffer(ab), Error);
assertEq(f(), 42);

assertThrowsInstanceOf(() => serialize(ab, [ab]), Error);
assertEq(f(), 42);
