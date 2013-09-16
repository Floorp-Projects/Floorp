load(libdir + "asm.js");

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[0>>2] = 4.0; return i32[0>>2]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[0>>2] = 4; return +f32[0>>2]; } return f');

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { var x=0,y=0; return i8[x+y]|0 } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { var x=0,y=0; return u8[x+y]|0 } return f');

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[0>>0]|0 }; return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[0>>1]|0 }; return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[0>>4]|0 }; return f');
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[0]|0 }; return f'), this, null, new ArrayBuffer(4096))(), 0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i = i32[i>>2]|0; return i|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0), 0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return i8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),-1);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return u8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),0xff);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return i16[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7fff),0x7fff);
assertEq(f(0xffff),-1);
assertEq(f(0x10000),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return u16[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7fff),0x7fff);
assertEq(f(0xffff),0xffff);
assertEq(f(0x10000),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return i32[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7fffffff),0x7fffffff);
assertEq(f(0xffffffff),-1);
assertEq(f(0x100000000),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return u32[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7fffffff),0x7fffffff);
assertEq(f(0xffffffff),-1);
assertEq(f(0x100000000),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return i8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),-1);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i32[0] = i; return u8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),0xff);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i8[0] = i; return i8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),-1);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; i8[0] = i; return u8[0]|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0),0);
assertEq(f(0x7f),0x7f);
assertEq(f(0xff),0xff);
assertEq(f(0x100),0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=+j; f64[i>>3] = j; return (~~+f64[i>>3])|0}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0, 1.3), 1);
assertEq(f(4088, 2.5), 2);
assertEq(f(4096, 3.8), 0);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=+j; f64[i>>3] = j; return +f64[i>>3]}; return f');
var f = asmLink(code, this, null, new ArrayBuffer(4096));
assertEq(f(0, 1.3), 1.3);
assertEq(f(4088, 2.5), 2.5);
assertEq(f(4096, 3.8), NaN);

var i32 = new Int32Array(4096);
i32[0] = 42;
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[1] = i32[0] }; return f'), this, null, i32.buffer)();
assertEq(i32[1], 42);

var f64 = new Float64Array(4096);
f64[0] = 42;
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[1] = f64[0] }; return f'), this, null, f64.buffer)();
assertEq(f64[1], 42);

var i32 = new Int32Array(4096/4);
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

asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[7&0xffff] = 41 } return f'), this, null, BUF_64KB)();
assertEq(new Uint8Array(BUF_64KB)[7], 41);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[7&0xffff] = -41 } return f'), this, null, BUF_64KB)();
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
new Float64Array(BUF_64KB)[1] = 1.3;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[(8&0xffff)>>3] } return f'), this, null, BUF_64KB)(), 1.3);

asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u8[255]; u8[i] } return f');
asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u8[i&0xff]; u8[255] } return f');
asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u32[63]; u32[i>>2] } return f');
asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u32[i>>2]; u32[63] } return f');

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u32[64] } return f');
asmLink(code, this, null, new ArrayBuffer(4096));

asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; u32[12] = i } return f'), this, null, BUF_64KB)(11);
assertEq(new Int32Array(BUF_64KB)[12], 11);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[12]|0 } return f'), this, null, BUF_64KB)(), 11);
new Float64Array(BUF_64KB)[0] = 3.5;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +-f64[0] } return f'), this, null, BUF_64KB)(), -3.5);

// Test constant loads.
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[1] = -1 } return f'), this, null, buf)();
assertEq(new Uint8Array(buf)[0], 0);
assertEq(new Uint8Array(buf)[1], 255);
assertEq(new Uint8Array(buf)[2], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[4095] = -1 } return f'), this, null, buf)();
assertEq(new Uint8Array(buf)[4094], 0);
assertEq(new Uint8Array(buf)[4095], 255);
assertEq(new Uint8Array(buf)[4096], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[4096] = -1 } return f'), this, null, buf)();
assertEq(new Uint8Array(buf)[4095], 0);
assertEq(new Uint8Array(buf)[4096], 255);
assertEq(new Uint8Array(buf)[4097], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[8192] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[258048] = -1 } return f'), this, null, buf)();
assertEq(new Uint8Array(buf)[258047], 0);
assertEq(new Uint8Array(buf)[258048], 255);
assertEq(new Uint8Array(buf)[258049], 0);

var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[1] = -1 } return f'), this, null, buf)();
assertEq(new Int8Array(buf)[0], 0);
assertEq(new Int8Array(buf)[1], -1);
assertEq(new Int8Array(buf)[2], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[4095] = -1 } return f'), this, null, buf)();
assertEq(new Int8Array(buf)[4094], 0);
assertEq(new Int8Array(buf)[4095], -1);
assertEq(new Int8Array(buf)[4096], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[4096] = -1 } return f'), this, null, buf)();
assertEq(new Int8Array(buf)[4095], 0);
assertEq(new Int8Array(buf)[4096], -1);
assertEq(new Int8Array(buf)[4097], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[8192] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[258048] = -1 } return f'), this, null, buf)();
assertEq(new Int8Array(buf)[258047], 0);
assertEq(new Int8Array(buf)[258048], -1);
assertEq(new Int8Array(buf)[258049], 0);

var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[1] = -1 } return f'), this, null, buf)();
assertEq(new Uint16Array(buf)[0], 0);
assertEq(new Uint16Array(buf)[1], 65535);
assertEq(new Uint16Array(buf)[2], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[2047] = -1 } return f'), this, null, buf)();
assertEq(new Uint16Array(buf)[2046], 0);
assertEq(new Uint16Array(buf)[2047], 65535);
assertEq(new Uint16Array(buf)[2048], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[2048] = -1 } return f'), this, null, buf)();
assertEq(new Uint16Array(buf)[2047], 0);
assertEq(new Uint16Array(buf)[2048], 65535);
assertEq(new Uint16Array(buf)[2049], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[4096] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[126976] = -1 } return f'), this, null, buf)();
assertEq(new Uint16Array(buf)[126975], 0);
assertEq(new Uint16Array(buf)[126976], 65535);
assertEq(new Uint16Array(buf)[126977], 0);

var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[1] = -1 } return f'), this, null, buf)();
assertEq(new Int16Array(buf)[0], 0);
assertEq(new Int16Array(buf)[1], -1);
assertEq(new Int16Array(buf)[2], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[2047] = -1 } return f'), this, null, buf)();
assertEq(new Int16Array(buf)[2046], 0);
assertEq(new Int16Array(buf)[2047], -1);
assertEq(new Int16Array(buf)[2048], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[2048] = -1 } return f'), this, null, buf)();
assertEq(new Int16Array(buf)[2047], 0);
assertEq(new Int16Array(buf)[2048], -1);
assertEq(new Int16Array(buf)[2049], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[4096] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[126976] = -1 } return f'), this, null, buf)();
assertEq(new Int16Array(buf)[126975], 0);
assertEq(new Int16Array(buf)[126976], -1);
assertEq(new Int16Array(buf)[126977], 0);

var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[1] = -1 } return f'), this, null, buf)();
assertEq(new Uint32Array(buf)[0], 0);
assertEq(new Uint32Array(buf)[1], 4294967295);
assertEq(new Uint32Array(buf)[2], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[1023] = -1 } return f'), this, null, buf)();
assertEq(new Uint32Array(buf)[1022], 0);
assertEq(new Uint32Array(buf)[1023], 4294967295);
assertEq(new Uint32Array(buf)[1024], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[1024] = -1 } return f'), this, null, buf)();
assertEq(new Uint32Array(buf)[1023], 0);
assertEq(new Uint32Array(buf)[1024], 4294967295);
assertEq(new Uint32Array(buf)[1025], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[2048] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[61440] = -1 } return f'), this, null, buf)();
assertEq(new Uint32Array(buf)[61439], 0);
assertEq(new Uint32Array(buf)[61440], 4294967295);
assertEq(new Uint32Array(buf)[61441], 0);

var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[1] = -1 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[0], 0);
assertEq(new Int32Array(buf)[1], -1);
assertEq(new Int32Array(buf)[2], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[1023] = -1 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[1022], 0);
assertEq(new Int32Array(buf)[1023], -1);
assertEq(new Int32Array(buf)[124], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[1024] = -1 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[1023], 0);
assertEq(new Int32Array(buf)[1024], -1);
assertEq(new Int32Array(buf)[1025], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[2048] = -1 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[61440] = -1 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[61439], 0);
assertEq(new Int32Array(buf)[61440], -1);
assertEq(new Int32Array(buf)[61441], 0);

var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[1] = -1.0 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[0], 0);
assertEq(new Float32Array(buf)[1], -1.0);
assertEq(new Int32Array(buf)[2], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[1023] = -1.0 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[1022], 0);
assertEq(new Float32Array(buf)[1023], -1.0);
assertEq(new Int32Array(buf)[124], 0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[1024] = -1.0 } return f'), this, null, buf)();
assertEq(new Int32Array(buf)[1023], 0);
assertEq(new Float32Array(buf)[1024], -1.0);
assertEq(new Int32Array(buf)[1025], 0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[2048] = -1.0 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[61440] = -1.0 } return f'), this, null, buf)();
assertEq(new Float32Array(buf)[61439], 0.0);
assertEq(new Float32Array(buf)[61440], -1.0);
assertEq(new Float32Array(buf)[61441], 0.0);

var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[1] = -1.0 } return f'), this, null, buf)();
assertEq(new Float64Array(buf)[0], 0.0);
assertEq(new Float64Array(buf)[1], -1.0);
assertEq(new Float64Array(buf)[2], 0.0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[511] = -1.0 } return f'), this, null, buf)();
assertEq(new Float64Array(buf)[510], 0.0);
assertEq(new Float64Array(buf)[511], -1.0);
assertEq(new Float64Array(buf)[512], 0.0);
var buf = new ArrayBuffer(8192);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[512] = -1.0 } return f'), this, null, buf)();
assertEq(new Float64Array(buf)[511], 0.0);
assertEq(new Float64Array(buf)[512], -1.0);
assertEq(new Float64Array(buf)[513], 0.0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[1024] = -1.0 } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[28672] = -1.0 } return f'), this, null, buf)();
assertEq(new Float64Array(buf)[28671], 0.0);
assertEq(new Float64Array(buf)[28672], -1.0);
assertEq(new Float64Array(buf)[28673], 0.0);


var buf = new ArrayBuffer(8192);
new Uint8Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[1]|0; } return f'), this, null, buf)(),255);
new Int8Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i8[1]|0; } return f'), this, null, buf)(),-1);
var buf = new ArrayBuffer(262144);
new Uint8Array(buf)[126976] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[126976]|0; } return f'), this, null, buf)(),255);
new Int8Array(buf)[126976] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i8[126976]|0; } return f'), this, null, buf)(),-1);

var buf = new ArrayBuffer(8192);
new Uint16Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u16[1]|0; } return f'), this, null, buf)(),65535);
new Int16Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i16[1]|0; } return f'), this, null, buf)(),-1);
var buf = new ArrayBuffer(262144);
new Uint16Array(buf)[126976] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u16[126976]|0; } return f'), this, null, buf)(),65535);
new Int16Array(buf)[126976] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i16[126976]|0; } return f'), this, null, buf)(),-1);

var buf = new ArrayBuffer(8192);
new Uint32Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[1]|0; } return f'), this, null, buf)(),-1);
new Int32Array(buf)[1] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[1]|0; } return f'), this, null, buf)(),-1);
var buf = new ArrayBuffer(262144);
new Int32Array(buf)[61440] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[61440]|0; } return f'), this, null, buf)(),-1);

var buf = new ArrayBuffer(8192);
new Float32Array(buf)[1] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[1]; } return f'), this, null, buf)(),-1.0);
new Float32Array(buf)[1023] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[1023]; } return f'), this, null, buf)(),-1.0);
new Float32Array(buf)[1024] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[1024]; } return f'), this, null, buf)(),-1.0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[2048]; } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
new Float32Array(buf)[61440] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[61440]; } return f'), this, null, buf)(),-1.0);

var buf = new ArrayBuffer(8192);
new Float64Array(buf)[1] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[1]; } return f'), this, null, buf)(),-1.0);
new Float64Array(buf)[511] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[511]; } return f'), this, null, buf)(),-1.0);
new Float64Array(buf)[512] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[512]; } return f'), this, null, buf)(),-1.0);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[1024]; } return f'), this, null, buf);
var buf = new ArrayBuffer(262144);
new Float64Array(buf)[28672] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[28672]; } return f'), this, null, buf)(),-1.0);

// Test bitwise-and optimizations.
var buf = new ArrayBuffer(8192);
new Uint8Array(buf)[8191] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[8191&8191]|0; } return f'), this, null, buf)(),255);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[(8191&8191)>>0]|0; } return f'), this, null, buf)(),255);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[8192&8191] = -1; u8[0] = 0; return u8[8192&8191]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u8[(8192&8191)>>0] = -1; u8[0] = 0; return u8[(8192&8191)>>0]|0; } return f'), this, null, buf)(),0);
new Int8Array(buf)[8191] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i8[8191&8191]|0; } return f'), this, null, buf)(),-1);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i8[(8191&8191)>>0]|0; } return f'), this, null, buf)(),-1);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[8192&8191] = -1; i8[0] = 0; return i8[8192&8191]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i8[(8192&8191)>>0] = -1; i8[0] = 0; return i8[(8192&8191)>>0]|0; } return f'), this, null, buf)(),0);
var buf = new ArrayBuffer(8192);
new Uint16Array(buf)[4095] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u16[(8190&8191)>>1]|0; } return f'), this, null, buf)(),65535);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u16[(8191&8191)>>1]|0; } return f'), this, null, buf)(),65535);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u16[(8192&8191)>>1] = -1; u16[0] = 0; return u16[(8192&8191)>>1]|0; } return f'), this, null, buf)(),0);
new Int16Array(buf)[4095] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i16[(8190&8191)>>1]|0; } return f'), this, null, buf)(),-1);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i16[(8191&8191)>>1]|0; } return f'), this, null, buf)(),-1);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i16[(8192&8191)>>1] = -1; i16[0] = 0; return i16[(8192&8191)>>1]|0; } return f'), this, null, buf)(),0);

var buf = new ArrayBuffer(8192);
new Uint32Array(buf)[2047] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(8188&8191)>>2]|0; } return f'), this, null, buf)(),-1);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(8191&8191)>>2]|0; } return f'), this, null, buf)(),-1);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { u32[(8192&8191)>>2] = -1; u32[0] = 0; return u32[(8192&8191)>>2]|0; } return f'), this, null, buf)(),0);
new Int32Array(buf)[2047] = -1;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[(8188&8191)>>2]|0; } return f'), this, null, buf)(),-1);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[(8191&8191)>>2]|0; } return f'), this, null, buf)(),-1);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { i32[(8192&8191)>>2] = -1; i32[0] = 0; return i32[(8192&8191)>>2]|0; } return f'), this, null, buf)(),0);

var buf = new ArrayBuffer(8192);
new Float32Array(buf)[2047] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[(8188&8191)>>2]; } return f'), this, null, buf)(),-1.0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f32[(8191&8191)>>2]; } return f'), this, null, buf)(),-1.0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f32[(8192&8191)>>2] = -1.0; f32[0] = 0.0; return +f32[(8192&8191)>>2]; } return f'), this, null, buf)(),0.0);

var buf = new ArrayBuffer(8192);
new Float64Array(buf)[1023] = -1.0;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[(8184&8191)>>3]; } return f'), this, null, buf)(),-1.0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[(8191&8191)>>3]; } return f'), this, null, buf)(),-1.0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { f64[(8192&8191)>>3] = -1.0; f64[0] = 0.0; return +f64[(8192&8191)>>3]; } return f'), this, null, buf)(),0.0);

// Bug 913867
var buf = new ArrayBuffer(8192);
new Int32Array(buf)[0] = 0x55aa5a5a;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[(0&0)>>2]|0; } return f'), this, null, buf)(),0x55aa5a5a);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return i32[(4&0)>>2]|0; } return f'), this, null, buf)(),0x55aa5a5a);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f1() { i32[0] = 1; return 8; }; function f() { return i32[((f1()|0)&0)>>2]|0; } return f'), this, null, buf)(),1);
assertEq(new Int32Array(buf)[0], 1);


// Bug 882012
assertEq(asmLink(asmCompile('stdlib', 'foreign', 'heap', USE_ASM + "var id=foreign.id;var doubles=new stdlib.Float64Array(heap);function g(){doubles[0]=+id(2.0);return +doubles[0];}return g"), this, {id: function(x){return x;}}, BUF_64KB)(), 2.0);


// Some literal constant paths.

var buf = new ArrayBuffer(8192);
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[0>>4294967295]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[0>>-1]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[0>>0x80000000]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[0>>-2147483648]|0; } return f');

new Uint32Array(buf)[0] = 0xAA;
new Uint32Array(buf)[0x5A>>2] = 0xA5;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(0x5A&4294967295)>>2]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=4294967295; function f() { return u32[(0x5A&i)>>2]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(0x5A&-1)>>2]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-1; function f() { return u32[(0x5A&i)>>2]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(0x5A&0x80000000)>>2]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=0x80000000; function f() { return u32[(0x5A&i)>>2]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(0x5A&-2147483648)>>2]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-2147483648; function f() { return u32[(0x5A&i)>>2]|0; } return f'), this, null, buf)(),0xAA);

assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(4294967295&0x5A)>>2]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=4294967295; function f() { return u32[(i&0x5A)>>2]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(-1&0x5A)>>2]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-1; function f() { return u32[(i&0x5A)>>2]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(0x80000000&0x5A)>>2]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=0x80000000; function f() { return u32[(i&0x5A)>>2]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[(-2147483648&0x5A)>>2]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-2147483648; function f() { return u32[(-2147483648&0x5A)>>2]|0; } return f'), this, null, buf)(),0xAA);

assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[4294967295>>2]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=4294967295; function f() { return u32[i>>2]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[-1>>2]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-1; function f() { return u32[i>>2]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[0x80000000>>2]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=0x80000000; function f() { return u32[i>>2]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u32[-2147483648>>2]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-2147483648; function f() { return u32[-2147483648>>2]|0; } return f'), this, null, buf)(),0);

var buf = new ArrayBuffer(8192);
new Uint8Array(buf)[0] = 0xAA;
new Uint8Array(buf)[0x5A] = 0xA5;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[0x5A&4294967295]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=4294967295; function f() { return u8[0x5A&i]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[0x5A&-1]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-1; function f() { return u8[0x5A&i]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[0x5A&0x80000000]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=0x80000000; function f() { return u8[0x5A&i]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[0x5A&-2147483648]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-2147483648; function f() { return u8[0x5A&i]|0; } return f'), this, null, buf)(),0xAA);

assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[4294967295&0x5A]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=4294967295; function f() { return u8[i&0x5A]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[-1&0x5A]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-1; function f() { return u8[i&0x5A]|0; } return f'), this, null, buf)(),0xA5);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[0x80000000&0x5A]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=0x80000000; function f() { return u8[i&0x5A]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[-2147483648&0x5A]|0; } return f'), this, null, buf)(),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-2147483648; function f() { return u8[i&0x5A]|0; } return f'), this, null, buf)(),0xAA);

assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[4294967295]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=4294967295; function f() { return u8[i]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[-1]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-1; function f() { return u8[i]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[0x80000000]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=0x80000000; function f() { return u8[i]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[-2147483648]|0; } return f');
assertAsmTypeFail('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-2147483648; function f() { return u8[i]|0; } return f');

assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[4294967295>>0]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=4294967295; function f() { return u8[i>>0]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[-1>>0]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-1; function f() { return u8[i>>0]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[0x80000000>>0]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=0x80000000; function f() { return u8[i>>0]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return u8[-2147483648>>0]|0; } return f'), this, null, buf)(),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'const i=-2147483648; function f() { return u8[i>>0]|0; } return f'), this, null, buf)(),0);


// Heap length constraints
var buf = new ArrayBuffer(0x0fff);
assertAsmLinkAlwaysFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
var buf = new ArrayBuffer(0x1000);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
var buf = new ArrayBuffer(0x1010);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
var buf = new ArrayBuffer(0x2000);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
var buf = new ArrayBuffer(0xfe000);
new Uint8Array(buf)[0xfdfff] = 0xAA;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0xfdfff),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0xfe000),0);
var buf = new ArrayBuffer(0xfe010);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
var buf = new ArrayBuffer(0xff000);
new Uint8Array(buf)[0xfefff] = 0xAA;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0xfefff),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0xff000),0);
var buf = new ArrayBuffer(0xff800);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
var buf = new ArrayBuffer(0x100000);
new Uint8Array(buf)[0xfffff] = 0xAA;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0xfffff),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x100000),0);
var buf = new ArrayBuffer(0x3f8000);
new Uint8Array(buf)[0x3f7fff] = 0xAA;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x3f7fff),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x3f8000),0);
var buf = new ArrayBuffer(0x3fc000); // 4080K
new Uint8Array(buf)[0x3fbfff] = 0xAA;
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x3fbfff),0xAA);
assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x3fc000),0);
var buf = new ArrayBuffer(0x3fe000);
assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
// The rest are getting too large for regular testing.
//var buf = new ArrayBuffer(0xfe8000);
//assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
//var buf = new ArrayBuffer(0xff0000); // 16302K
//new Uint8Array(buf)[0xfeffff] = 0xAA;
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0xfeffff),0xAA);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0xff0000),0);
//var buf = new ArrayBuffer(0xff8000);
//assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
//var buf = new ArrayBuffer(0x3fb0000);
//assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
//var buf = new ArrayBuffer(0x3fc0000); // 65280K
//new Uint8Array(buf)[0x3fbffff] = 0xAA;
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x3fbffff),0xAA);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x3fc0000),0);
//var buf = new ArrayBuffer(0x3fe0000);
//assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
//var buf = new ArrayBuffer(0xfe80000);
//assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
//var buf = new ArrayBuffer(0xff00000); // 255M
//new Uint8Array(buf)[0xfeffff] = 0xAA;
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0xfeffff),0xAA);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0xff00000),0);
//var buf = new ArrayBuffer(0xff80000);
//assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
//var buf = new ArrayBuffer(0x3fa00000);
//assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
//var buf = new ArrayBuffer(0x3fc00000); // 1020M
//new Uint8Array(buf)[0x3fbfffff] = 0xAA;
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x3fbfffff),0xAA);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x3fc00000),0);
//var buf = new ArrayBuffer(0x3fe00000);
//assertAsmLinkFail(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf);
//var buf = new ArrayBuffer(0x40000000); // 1024M
//new Uint8Array(buf)[0x3fffffff] = 0xAA;
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x3fffffff),0xAA);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x40000000),0);
//var buf = new ArrayBuffer(0x4f000000); // 1264M - currently the largest possible heap length.
//new Uint8Array(buf)[0x4effffff] = 0xAA;
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0),0);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x4effffff),0xAA);
//assertEq(asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) { i=i|0; return u8[i]|0; } return f'), this, null, buf)(0x4f000000),0);

