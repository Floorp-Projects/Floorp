load(libdir + "asm.js");

var code = asmCompile(USE_ASM + "function g() { return 42 } return g");
assertEq(asmLink(code)(), 42);
assertEq(asmLink(code)(), 42);

var code = asmCompile('glob', 'ffis', 'buf', USE_ASM + 'var i32=new glob.Int32Array(buf); function g() { return i32[0]|0 } return g');
var i32_1 = new Int32Array(BUF_MIN/4);
i32_1[0] = 42;
var i32_2 = new Int32Array(BUF_MIN/4);
i32_2[0] = 13;
assertEq(asmLink(code, this, null, i32_1.buffer)(), 42);
assertEq(asmLink(code, this, null, i32_2.buffer)(), 13);
var i32_3 = new Int32Array(4097);
assertAsmLinkFail(code, this, null, i32_3.buffer);

var code = asmCompile('glob', 'ffis', USE_ASM + 'var ffi=ffis.ffi; function g(n) { n=n|0; var i=0; for(; (i|0)<(n|0); i=(i+1)|0) ffi() } return g');
var calls1 = 0, calls2 = 0;
function ffi1() { calls1++ }
function ffi2() { calls2++ }
asmLink(code, null, {ffi:ffi1})(100000);
assertEq(calls1, 100000);
assertEq(calls2, 0);
calls1 = 0;
asmLink(code, null, {ffi:ffi2})(50000);
assertEq(calls1, 0);
assertEq(calls2, 50000);

var code = asmCompile(USE_ASM + 'var g = 0; function h() { g=(g+1)|0; return g|0 } return h');
var h1 = code();
assertEq(h1(), 1);
assertEq(h1(), 2);
var h2 = code();
assertEq(h2(), 1);
assertEq(h1(), 3);
assertEq(h2(), 2);
assertEq(h1(), 4);

var code = asmCompile(USE_ASM + "return {}");
var h1 = code();
var h2 = code();
assertEq(h1 === h2, false);
assertEq(Object.keys(h1).length, 0);
assertEq(Object.keys(h2).length, 0);
