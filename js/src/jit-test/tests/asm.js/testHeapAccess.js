load(libdir + "asm.js");

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
