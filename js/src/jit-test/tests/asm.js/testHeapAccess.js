// |jit-test| test-also-noasmjs
load(libdir + "asm.js");

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[0>>2] = 4.0; return i32[0>>2]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[0>>2] = 4; return +f32[0>>2]; } return f');

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { var x=0,y=0; return i8[x+y]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { var x=0,y=0; return u8[x+y]|0 } return f');

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[0>>0]|0 }; return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[0>>1]|0 }; return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[0>>4]|0 }; return f');
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[0]|0 }; return f'), this, null, new ArrayBuffer(BUF_MIN))(), 0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i = i32[i>>2]|0; return i|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0), 0);

setCachingEnabled(true);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return i8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),-1);
assertEq(f(0x100),0);

{
      var buf = new ArrayBuffer(BUF_MIN);
      var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + '/* not a clone */ function f(i) {i=i|0; i32[0] = i; return i8[0]|0}; return f');
      var f = asmLink(code, this, null, buf);
      assertEq(f(0),0);
      assertEq(f(0x7f),0x7f);
      assertEq(f(0xff),-1);
      assertEq(f(0x100),0);

      // Bug 1088655
      assertEq(asmLink(asmCompile('stdlib', 'foreign', 'heap', USE_ASM + 'var i32=new stdlib.Int32Array(heap); function f(i) {i=i|0;var j=0x10000;return (i32[j>>2] = i)|0 } return f'), this, null, buf)(1), 1);
}

setCachingEnabled(false);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return u8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),0xff);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return i16[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7fff),0x7fff);
assertEq(f(0xffff),-1);
assertEq(f(0x10000),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return u16[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7fff),0x7fff);
assertEq(f(0xffff),0xffff);
assertEq(f(0x10000),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return i32[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7fffffff),0x7fffffff);
assertEq(f(0xffffffff),-1);
assertEq(f(0x100000000),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return u32[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7fffffff),0x7fffffff);
assertEq(f(0xffffffff),-1);
assertEq(f(0x100000000),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return i8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),-1);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return u8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),0xff);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i8[0] = i; return i8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),-1);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i8[0] = i; return u8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),0xff);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=+j; f64[i>>3] = j; return (~~+f64[i>>3])|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0, 1.3), 1);
assertEq(f(BUF_MIN-8, 2.5), 2);
assertEq(f(BUF_MIN, 3.8), 0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=+j; f64[i>>3] = j; return (~~f64[i>>3])|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0, 1.3), 1);
assertEq(f(BUF_MIN-8, 2.5), 2);
assertEq(f(BUF_MIN, 3.8), 0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=+j; f64[i>>3] = j; return +f64[i>>3]}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(BUF_MIN));
assertEq(f(0, 1.3), 1.3);
assertEq(f(BUF_MIN-8, 2.5), 2.5);
assertEq(f(BUF_MIN, 3.8), NaN);

var i32 = new Int32Array(BUF_MIN);
i32[0] = 42;
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[1] = i32[0] }; return f'), this, null, i32.buffer)();
assertEq(i32[1], 42);

var f64 = new Float64Array(BUF_MIN);
f64[0] = 42;
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[1] = f64[0] }; return f'), this, null, f64.buffer)();
assertEq(f64[1], 42);

var i32 = new Int32Array(BUF_MIN/4);
i32[0] = 13;
i32[1] = 0xfffeeee;
var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; return i32[((i<<2)+1)>>2]|0 }; return f'), this, null, i32.buffer);
assertEq(f(0), 13);
assertEq(f(1), 0xfffeeee);
var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; return i32[((i<<2)+2)>>2]|0 }; return f'), this, null, i32.buffer);
assertEq(f(0), 13);
assertEq(f(1), 0xfffeeee);
var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; return i32[(i<<1)>>2]|0 }; return f'), this, null, i32.buffer);
assertEq(f(0), 13);
assertEq(f(1), 13);
assertEq(f(2), 0xfffeeee);
assertEq(f(3), 0xfffeeee);

var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; return i32[(i<<2)>>2]|0 }; return f'), this, null, i32.buffer);
assertEq(f(0), 13);
assertEq(f(1), 0xfffeeee);

var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; return i32[((i<<2)+4)>>2]|0 }; return f'), this, null, i32.buffer);
assertEq(f(0), 0xfffeeee);

// For legacy compatibility, test Int8/Uint8 accesses with no shift.
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[7&0xffff] = 41 } return f'), this, null, BUF_64KB)();
assertEq(new Uint8Array(BUF_64KB)[7], 41);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[7&0xffff] = -41 } return f'), this, null, BUF_64KB)();
assertEq(new Int8Array(BUF_64KB)[7], -41);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[(7&0xffff)>>0] = 41 } return f'), this, null, BUF_64KB)();
assertEq(new Uint8Array(BUF_64KB)[7], 41);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[(7&0xffff)>>0] = -41 } return f'), this, null, BUF_64KB)();
assertEq(new Int8Array(BUF_64KB)[7], -41);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[(6&0xffff)>>1] = 0xabc } return f'), this, null, BUF_64KB)();
assertEq(new Uint16Array(BUF_64KB)[3], 0xabc);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[(6&0xffff)>>1] = -0xabc } return f'), this, null, BUF_64KB)();
assertEq(new Int16Array(BUF_64KB)[3], -0xabc);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[(4&0xffff)>>2] = 0xabcde } return f'), this, null, BUF_64KB)();
assertEq(new Uint32Array(BUF_64KB)[1], 0xabcde);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[(4&0xffff)>>2] = -0xabcde } return f'), this, null, BUF_64KB)();
assertEq(new Int32Array(BUF_64KB)[1], -0xabcde);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[(4&0xffff)>>2] = 1.0 } return f'), this, null, BUF_64KB)();
assertEq(new Float32Array(BUF_64KB)[1], 1.0);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[(8&0xffff)>>3] = 1.3 } return f'), this, null, BUF_64KB)();
assertEq(new Float64Array(BUF_64KB)[1], 1.3);

new Float32Array(BUF_64KB)[1] = 1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[(4&0xffff)>>2] } return f'), this, null, BUF_64KB)(), 1.0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'var toFloat32 = glob.Math.fround; function f() { return toFloat32(f32[(4&0xffff)>>2]) } return f'), this, null, BUF_64KB)(), 1.0);
new Float64Array(BUF_64KB)[1] = 1.3;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[(8&0xffff)>>3] } return f'), this, null, BUF_64KB)(), 1.3);

asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u8[255]; u8[i] } return f');
// For legacy compatibility, test Int8/Uint8 accesses with no shift.
asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u8[i&0xff]; u8[255] } return f');
asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u8[(i&0xff)>>0]; u8[255] } return f');
asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u32[63]; u32[i>>2] } return f');
asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u32[i>>2]; u32[63] } return f');

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u32[64] } return f');
asmLink(code, this, null, new ArrayBuffer(BUF_MIN));

asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u32[12] = i } return f'), this, null, BUF_64KB)(11);
assertEq(new Int32Array(BUF_64KB)[12], 11);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[12]|0 } return f'), this, null, BUF_64KB)(), 11);
new Float64Array(BUF_64KB)[0] = 3.5;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +-f64[0] } return f'), this, null, BUF_64KB)(), -3.5);

// Test constant loads.
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[1] = -1 } return f'), this, null, buf)();
assertEq(new Uint8Array(buf)[0], 0);
assertEq(new Uint8Array(buf)[1], 255);
assertEq(new Uint8Array(buf)[2], 0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[4095] = -1 } return f'), this, null, buf)();
assertEq(new Uint8Array(buf)[4094], 0);
assertEq(new Uint8Array(buf)[4095], 255);
assertEq(new Uint8Array(buf)[4096], 0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[4096] = -1 } return f'), this, null, buf)();
assertEq(new Uint8Array(buf)[4095], 0);
assertEq(new Uint8Array(buf)[4096], 255);
assertEq(new Uint8Array(buf)[4097], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[' + BUF_MIN + '] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[258048] = -1 } return f'), this, null, buf)();
assertEq(new Uint8Array(buf)[258047], 0);
assertEq(new Uint8Array(buf)[258048], 255);
assertEq(new Uint8Array(buf)[258049], 0);

var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[1] = -1 } return f'), this, null, buf)();
assertEq(new Int8Array(buf)[0], 0);
assertEq(new Int8Array(buf)[1], -1);
assertEq(new Int8Array(buf)[2], 0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[4095] = -1 } return f'), this, null, buf)();
assertEq(new Int8Array(buf)[4094], 0);
assertEq(new Int8Array(buf)[4095], -1);
assertEq(new Int8Array(buf)[4096], 0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[4096] = -1 } return f'), this, null, buf)();
assertEq(new Int8Array(buf)[4095], 0);
assertEq(new Int8Array(buf)[4096], -1);
assertEq(new Int8Array(buf)[4097], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[' + BUF_MIN + '] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[258048] = -1 } return f'), this, null, buf)();
assertEq(new Int8Array(buf)[258047], 0);
assertEq(new Int8Array(buf)[258048], -1);
assertEq(new Int8Array(buf)[258049], 0);

var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[1] = -1 } return f'), this, null, buf)();
assertEq(new Uint16Array(buf)[0], 0);
assertEq(new Uint16Array(buf)[1], 65535);
assertEq(new Uint16Array(buf)[2], 0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[' + (BUF_MIN/2-1) + '] = -1 } return f'), this, null, buf)();
assertEq(new Uint16Array(buf)[BUF_MIN/2-2], 0);
assertEq(new Uint16Array(buf)[BUF_MIN/2-1], 65535);
assertEq(new Uint16Array(buf)[BUF_MIN/2], undefined);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[' + (BUF_MIN/4) + '] = -1 } return f'), this, null, buf)();
assertEq(new Uint16Array(buf)[BUF_MIN/4-1], 0);
assertEq(new Uint16Array(buf)[BUF_MIN/4], 65535);
assertEq(new Uint16Array(buf)[BUF_MIN/4+1], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[' + (BUF_MIN/2) + '] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[126976] = -1 } return f'), this, null, buf)();
assertEq(new Uint16Array(buf)[126975], 0);
assertEq(new Uint16Array(buf)[126976], 65535);
assertEq(new Uint16Array(buf)[126977], 0);

var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[1] = -1 } return f'), this, null, buf)();
assertEq(new Int16Array(buf)[0], 0);
assertEq(new Int16Array(buf)[1], -1);
assertEq(new Int16Array(buf)[2], 0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[' + (BUF_MIN/2-1) + '] = -1 } return f'), this, null, buf)();
assertEq(new Int16Array(buf)[BUF_MIN/2-2], 0);
assertEq(new Int16Array(buf)[BUF_MIN/2-1], -1);
assertEq(new Int16Array(buf)[BUF_MIN/2], undefined);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[' + (BUF_MIN/4) + '] = -1 } return f'), this, null, buf)();
assertEq(new Int16Array(buf)[BUF_MIN/4-1], 0);
assertEq(new Int16Array(buf)[BUF_MIN/4], -1);
assertEq(new Int16Array(buf)[BUF_MIN/4+1], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[' + (BUF_MIN/2) + '] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[126976] = -1 } return f'), this, null, buf)();
assertEq(new Int16Array(buf)[126975], 0);
assertEq(new Int16Array(buf)[126976], -1);
assertEq(new Int16Array(buf)[126977], 0);

var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[1] = -1 } return f'), this, null, buf)();
assertEq(new Uint32Array(buf)[0], 0);
assertEq(new Uint32Array(buf)[1], 4294967295);
assertEq(new Uint32Array(buf)[2], 0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[' + (BUF_MIN/4-1) + '] = -1 } return f'), this, null, buf)();
assertEq(new Uint32Array(buf)[BUF_MIN/4-2], 0);
assertEq(new Uint32Array(buf)[BUF_MIN/4-1], 4294967295);
assertEq(new Uint32Array(buf)[BUF_MIN/4], undefined);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[' + (BUF_MIN/8) + '] = -1 } return f'), this, null, buf)();
assertEq(new Uint32Array(buf)[BUF_MIN/8-1], 0);
assertEq(new Uint32Array(buf)[BUF_MIN/8], 4294967295);
assertEq(new Uint32Array(buf)[BUF_MIN/8+1], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[' + (BUF_MIN/4) + '] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[61440] = -1 } return f'), this, null, buf)();
assertEq(new Uint32Array(buf)[61439], 0);
assertEq(new Uint32Array(buf)[61440], 4294967295);
assertEq(new Uint32Array(buf)[61441], 0);

var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[1] = -1 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[0], 0);
assertEq(new Int32Array(buf)[1], -1);
assertEq(new Int32Array(buf)[2], 0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[' + (BUF_MIN/4-1) + '] = -1 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[BUF_MIN/4-2], 0);
assertEq(new Int32Array(buf)[BUF_MIN/4-1], -1);
assertEq(new Int32Array(buf)[BUF_MIN/4], undefined);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[' + (BUF_MIN/8) + '] = -1 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[BUF_MIN/8-1], 0);
assertEq(new Int32Array(buf)[BUF_MIN/8], -1);
assertEq(new Int32Array(buf)[BUF_MIN/8+1], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[' + (BUF_MIN/4) + '] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[61440] = -1 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[61439], 0);
assertEq(new Int32Array(buf)[61440], -1);
assertEq(new Int32Array(buf)[61441], 0);

var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[1] = -1.0 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[0], 0);
assertEq(new Float32Array(buf)[1], -1.0);
assertEq(new Int32Array(buf)[2], 0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[' + (BUF_MIN/4-1) + '] = -1.0 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[BUF_MIN/4-2], 0);
assertEq(new Float32Array(buf)[BUF_MIN/4-1], -1.0);
assertEq(new Int32Array(buf)[BUF_MIN/4], undefined);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[' + (BUF_MIN/8) + '] = -1.0 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[BUF_MIN/8-1], 0);
assertEq(new Float32Array(buf)[BUF_MIN/8], -1.0);
assertEq(new Int32Array(buf)[BUF_MIN/8+1], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[' + (BUF_MIN/4) + '] = -1.0 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[61440] = -1.0 } return f'), this, null, buf)();
assertEq(new Float32Array(buf)[61439], 0.0);
assertEq(new Float32Array(buf)[61440], -1.0);
assertEq(new Float32Array(buf)[61441], 0.0);

var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[1] = -1.0 } return f'), this, null, buf)();
assertEq(new Float64Array(buf)[0], 0.0);
assertEq(new Float64Array(buf)[1], -1.0);
assertEq(new Float64Array(buf)[2], 0.0);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[' + (BUF_MIN/8-1) + '] = -1.0 } return f'), this, null, buf)();
assertEq(new Float64Array(buf)[BUF_MIN/8-2], 0.0);
assertEq(new Float64Array(buf)[BUF_MIN/8-1], -1.0);
assertEq(new Float64Array(buf)[BUF_MIN/8], undefined);
var buf = new ArrayBuffer(BUF_MIN);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[' + (BUF_MIN/16) + '] = -1.0 } return f'), this, null, buf)();
assertEq(new Float64Array(buf)[BUF_MIN/16-1], 0.0);
assertEq(new Float64Array(buf)[BUF_MIN/16], -1.0);
assertEq(new Float64Array(buf)[BUF_MIN/16+1], 0.0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[' + (BUF_MIN/8) + '] = -1.0 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[28672] = -1.0 } return f'), this, null, buf)();
assertEq(new Float64Array(buf)[28671], 0.0);
assertEq(new Float64Array(buf)[28672], -1.0);
assertEq(new Float64Array(buf)[28673], 0.0);


var buf = new ArrayBuffer(BUF_MIN);
new Uint8Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[1]|0; } return f'), this, null, buf)(),255);
new Int8Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i8[1]|0; } return f'), this, null, buf)(),-1);
var buf = new ArrayBuffer(262144);
new Uint8Array(buf)[126976] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[126976]|0; } return f'), this, null, buf)(),255);
new Int8Array(buf)[126976] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i8[126976]|0; } return f'), this, null, buf)(),-1);

var buf = new ArrayBuffer(BUF_MIN);
new Uint16Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u16[1]|0; } return f'), this, null, buf)(),65535);
new Int16Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i16[1]|0; } return f'), this, null, buf)(),-1);
var buf = new ArrayBuffer(262144);
new Uint16Array(buf)[126976] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u16[126976]|0; } return f'), this, null, buf)(),65535);
new Int16Array(buf)[126976] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i16[126976]|0; } return f'), this, null, buf)(),-1);

var buf = new ArrayBuffer(BUF_MIN);
new Uint32Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[1]|0; } return f'), this, null, buf)(),-1);
new Int32Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[1]|0; } return f'), this, null, buf)(),-1);
var buf = new ArrayBuffer(262144);
new Int32Array(buf)[61440] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[61440]|0; } return f'), this, null, buf)(),-1);

var buf = new ArrayBuffer(BUF_MIN);
new Float32Array(buf)[1] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[1]; } return f'), this, null, buf)(),-1.0);
new Float32Array(buf)[BUF_MIN/4-1] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[' + (BUF_MIN/4-1) + ']; } return f'), this, null, buf)(),-1.0);
new Float32Array(buf)[BUF_MIN/8] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[' + (BUF_MIN/8) + ']; } return f'), this, null, buf)(),-1.0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[' + (BUF_MIN/4) + ']; } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
new Float32Array(buf)[61440] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[61440]; } return f'), this, null, buf)(),-1.0);

var buf = new ArrayBuffer(BUF_MIN);
new Float64Array(buf)[1] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[1]; } return f'), this, null, buf)(),-1.0);
new Float64Array(buf)[BUF_MIN/8-1] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[' + (BUF_MIN/8-1) + ']; } return f'), this, null, buf)(),-1.0);
new Float64Array(buf)[BUF_MIN/16] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[' + (BUF_MIN/16) + ']; } return f'), this, null, buf)(),-1.0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[' + (BUF_MIN/8) + ']; } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
new Float64Array(buf)[28672] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[28672]; } return f'), this, null, buf)(),-1.0);

// Bug 913867
var buf = new ArrayBuffer(BUF_MIN);
new Int32Array(buf)[0] = 0x55aa5a5a;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f1() { i32[0] = 1; return 8; }; function f() { return i32[((f1()|0)&0)>>2]|0; } return f'), this, null, buf)(),1);
assertEq(new Int32Array(buf)[0], 1);

// Bug 882012
assertEq(asmLink(asmCompile('stdlib', 'foreign', 'heap', USE_ASM + "var id=foreign.id;var doubles=new stdlib.Float64Array(heap);function g(){doubles[0]=+id(2.0);return +doubles[0];}return g"), this, {id: function(x){return x;}}, BUF_64KB)(), 2.0);

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[4294967295]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=4294967295; function f() { return u8[i]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[-1]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-1; function f() { return u8[i]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[0x80000000]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=0x80000000; function f() { return u8[i]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[-2147483648]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-2147483648; function f() { return u8[i]|0; } return f');

// GVN checks
var buf = new ArrayBuffer(BUF_MIN);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'var i=0; function f() { var x = 0, y = 0; u8[0] = 1; u8[1] = 2; x = 0|u8[i]; i = x; y = 0|u8[i]; return y|0;} return f'), this, null, buf)(),2);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'var i=0; function f() { var x = 0, y = 0; u8[0] = 1; u8[1] = 2; x = 0|u8[i]; y = 0|u8[i]; return (x+y)|0;} return f'), this, null, buf)(),2);

// Heap length constraints
var m = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f');
assertAsmLinkAlwaysFail(m, this, null, new ArrayBuffer(0xffff));
assertEq(asmLink(m, this, null, new ArrayBuffer(0x10000))(0),0);
assertAsmLinkFail(m, this, null, new ArrayBuffer(0x10010));
assertEq(asmLink(m, this, null, new ArrayBuffer(0x20000))(0),0);
assertAsmLinkFail(m, this, null, new ArrayBuffer(0xfe0000));
assertAsmLinkFail(m, this, null, new ArrayBuffer(0xfe0010));
assertAsmLinkFail(m, this, null, new ArrayBuffer(0xfe0000));
assertAsmLinkFail(m, this, null, new ArrayBuffer(0xff8000));
var buf = new ArrayBuffer(0x100000);
new Uint8Array(buf)[0x4242] = 0xAA;
var f = asmLink(m, this, null, buf);
assertEq(f(0),0);
assertEq(f(0x4242),0xAA);
assertEq(f(0x100000),0);
assertAsmLinkFail(m, this, null, new ArrayBuffer(0x104000));
assertEq(asmLink(m, this, null, new ArrayBuffer(0x200000))(0),0);
assertAsmLinkFail(m, this, null, new ArrayBuffer(0x3f8000));
assertAsmLinkFail(m, this, null, new ArrayBuffer(0x3fe000));
assertAsmLinkFail(m, this, null, new ArrayBuffer(0x3fc000));
assertEq(asmLink(m, this, null, new ArrayBuffer(0x400000))(0),0);
assertAsmLinkFail(m, this, null, new ArrayBuffer(0x410000));
assertEq(asmLink(m, this, null, new ArrayBuffer(0x800000))(0),0);
var buf = new ArrayBuffer(0x1000000);
new Uint8Array(buf)[0x424242] = 0xAA;
var f = asmLink(m, this, null, buf);
assertEq(f(0),0);
assertEq(f(0x424242),0xAA);
assertEq(f(0x1000000),0);
assertEq(asmLink(m, this, null, new ArrayBuffer(0x2000000))(0),0);
assertEq(asmLink(m, this, null, new ArrayBuffer(0x3000000))(0),0);

// Heap offsets
var asmMod = function test (glob, env, b) {
    'use asm';
    var i8 = new glob.Int8Array(b);
    function f(i) {
        i = i | 0;
        i = i & 1;
        i = (i - 0x40000)|0;
        i8[0x3ffff] = 0;
        return i8[(i + 0x7fffe) >> 0] | 0;
    }
    return f;
};
var buffer = new ArrayBuffer(0x40000);
var asm = asmMod(this, {}, buffer);
assertEq(asm(-1),0);
