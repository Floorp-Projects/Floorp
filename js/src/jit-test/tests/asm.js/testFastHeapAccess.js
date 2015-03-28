// |jit-test| test-also-noasmjs
load(libdir + "asm.js");

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=j|0; u32[((i<<2)+32 & 0xffff)>>2] = j } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    f(i, i);
var u32 = new Uint32Array(BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(u32[8+i], i);
var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; return u32[((i<<2)+32 & 0xffff)>>2]|0 } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(f(i), i);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=j|0; u32[(i<<2 & 0xffff)>>2] = j } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    f(i, i);
var u32 = new Uint32Array(BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(u32[i], i);
var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; return u32[(i<<2 & 0xffff)>>2]|0 } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(f(i), i);

// For legacy compatibility, test Int8/Uint8 accesses with no shift.
var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=j|0; u8[i+20 & 0xffff] = j } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    f(i, i);
var u8 = new Uint8Array(BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(u8[i+20], i);
var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; return u8[i+20 & 0xffff]|0 } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(f(i), i);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=j|0; u8[(i+20 & 0xffff)>>0] = j } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    f(i, i);
var u8 = new Uint8Array(BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(u8[i+20], i);
var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i) {i=i|0; return u8[(i+20 & 0xffff)>>0]|0 } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(f(i), i);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j,k) {i=i|0;j=j|0;k=k|0; i32[(i + (j<<2) & 0xffff) >> 2] = k } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    f(32, i, i);
var u32 = new Uint32Array(BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(u32[8+i], i);
var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=j|0; return i32[(i + (j<<2) & 0xffff) >> 2]|0 } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(f(32, i), i);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j,k) {i=i|0;j=j|0;k=k|0; i32[(((i + (j<<2))|0) + 16 & 0xffff) >> 2] = k } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    f(32, i, i);
var u32 = new Uint32Array(BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(u32[8+i+4], i);
var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=j|0; return i32[(((i + (j<<2))|0) + 16 & 0xffff) >> 2]|0 } return f');
var f = asmLink(code, this, null, BUF_64KB);
for (var i = 0; i < 100; i++)
    assertEq(f(32, i), i);

var code = asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f(i,j) {i=i|0;j=j|0; return ((i32[(i+(j<<2)&0xffff)>>2]|0) + (i32[(((i+(j<<2))|0)+4&0xffff)>>2]|0))|0 } return f');
var f = asmLink(code, this, null, BUF_64KB);
var i32 = new Uint32Array(BUF_64KB);
i32[11] = 3;
i32[12] = 97;
assertEq(f(12,8), 100);
