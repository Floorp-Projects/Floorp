load(libdir + "asm.js");

assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0.0; return (i+j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0.0,j=0; return (i+j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0.0; return (i-j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0.0,j=0; return (i-j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0.0; return (i*j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0.0,j=0; return (i*j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0; return (i*j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0; return (i*1048576)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0; return (i*-1048576)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0; return (i + (j*4))|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var two30 = 1073741824; return (((two30 * 524288 * 16) + 1) & 1)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0.0; return (i/j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0.0,j=0; return (i/j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=1,j=1; return (i/j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0.0; return (i%j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0.0,j=0; return (i%j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0.0,j=0; return (i<j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0.0; return (i<j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0; return (i<j)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0.0; return (-i)|0 } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0; return (-(i+j))|0 } return f");

assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0,k=0; k = (i|0)/(k|0) } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0,k=0; k = (i>>>0)/(k>>>0) } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0,k=0; k = (i|0)%(k|0) } return f");
assertAsmTypeFail(USE_ASM + "function f() { var i=0,j=0,k=0; k = (i>>>0)%(k>>>0) } return f");

const UINT32_MAX = Math.pow(2,32)-1;
const INT32_MIN = -Math.pow(2,31);
const INT32_MAX = Math.pow(2,31)-1;

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return (i*2)|0 } return f"));
assertEq(f(0), 0);
assertEq(f(INT32_MIN), (2*INT32_MIN)|0);
assertEq(f(INT32_MAX), (2*INT32_MAX)|0);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return (2*i)|0 } return f"));
assertEq(f(0), 0);
assertEq(f(INT32_MIN), (2*INT32_MIN)|0);
assertEq(f(INT32_MAX), (2*INT32_MAX)|0);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return (i*1048575)|0 } return f"));
assertEq(f(0), 0);
assertEq(f(2), (1048575*2)|0);
assertEq(f(-1), (1048575*-1)|0);
assertEq(f(INT32_MIN), (1048575*INT32_MIN)|0);
assertEq(f(INT32_MAX), (1048575*INT32_MAX)|0);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return (1048575*i)|0 } return f"));
assertEq(f(0), 0);
assertEq(f(2), (1048575*2)|0);
assertEq(f(-1), (1048575*-1)|0);
assertEq(f(INT32_MIN), (1048575*INT32_MIN)|0);
assertEq(f(INT32_MAX), (1048575*INT32_MAX)|0);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=+i; var j=0; j=~~i; return j|0 } return f"));
assertEq(f(0), 0);
assertEq(f(3.5), 3);
assertEq(f(-3.5), -3);
assertEq(f(INT32_MAX), INT32_MAX);
assertEq(f(INT32_MIN), INT32_MIN);
assertEq(f(UINT32_MAX), -1);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; var j=0.0; j=+~~i; return +j } return f"));
assertEq(f(0), 0);
assertEq(f(INT32_MAX), INT32_MAX);
assertEq(f(INT32_MIN), INT32_MIN);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; var j=0.1; j=+(i>>>0); return +j } return f"));
assertEq(f(0), 0);
assertEq(f(INT32_MAX), INT32_MAX);
assertEq(f(UINT32_MAX), UINT32_MAX);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return (-i)|0 } return f"));
assertEq(f(0), 0);
assertEq(f(-0), 0);
assertEq(f(1), -1);
assertEq(f(INT32_MAX), INT32_MIN+1);
assertEq(f(INT32_MIN), INT32_MIN);

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=+i; return +(-i) } return f"));
assertEq(f(0), -0);
assertEq(f(-0), 0);
assertEq(f(-1), 1);
assertEq(f(1), -1);
assertEq(f(Math.pow(2,50)), -Math.pow(2,50));
assertEq(f(1.54e20), -1.54e20);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return ((i|0) < (j|0))|0 } return f"));
assertEq(f(0, 1), 1);
assertEq(f(1, 0), 0);
assertEq(f(1, 1), 0);
assertEq(f(INT32_MIN, INT32_MAX), 1);
assertEq(f(INT32_MAX, INT32_MIN), 0);
assertEq(f(0, INT32_MAX), 1);
assertEq(f(INT32_MAX, 0), 0);
assertEq(f(INT32_MIN, 0), 1);
assertEq(f(0, INT32_MIN), 0);
assertEq(f(UINT32_MAX, 0), 1);
assertEq(f(0, UINT32_MAX), 0);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return ((i>>>0) < (j>>>0))|0 } return f"));
assertEq(f(0, 1), 1);
assertEq(f(1, 0), 0);
assertEq(f(1, 1), 0);
assertEq(f(INT32_MIN, INT32_MAX), 0);
assertEq(f(INT32_MAX, INT32_MIN), 1);
assertEq(f(0, INT32_MAX), 1);
assertEq(f(INT32_MAX, 0), 0);
assertEq(f(INT32_MIN, 0), 0);
assertEq(f(0, INT32_MIN), 1);
assertEq(f(UINT32_MAX, 0), 0);
assertEq(f(0, UINT32_MAX), 1);

assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var k=0; k=(i|0)==(j|0); return k|0 } return f"))(1,2), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var k=0; k=(i|0)!=(j|0); return k|0 } return f"))(1,2), 1);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var k=0; k=(i|0)<(j|0); return k|0 } return f"))(1,2), 1);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var k=0; k=(i|0)>(j|0); return k|0 } return f"))(1,2), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var k=0; k=(i|0)<=(j|0); return k|0 } return f"))(1,2), 1);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var k=0; k=(i|0)>=(j|0); return k|0 } return f"))(1,2), 0);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return ((i|0)/(j|0))|0 } return f"));
assertEq(f(4,2), 2);
assertEq(f(3,2), 1);
assertEq(f(3,-2), -1);
assertEq(f(-3,-2), 1);
assertEq(f(0, -1), 0);
assertEq(f(0, INT32_MAX), 0);
assertEq(f(0, INT32_MIN), 0);
assertEq(f(INT32_MAX, 0), 0);
assertEq(f(INT32_MIN, 0), 0);
assertEq(f(-1, INT32_MAX), 0);
assertEq(f(-1, INT32_MIN), 0);
assertEq(f(INT32_MAX, -1), -INT32_MAX);
assertEq(f(INT32_MIN, -1), INT32_MIN); // !!
assertEq(f(INT32_MAX, INT32_MAX), 1);
assertEq(f(INT32_MAX, INT32_MIN), 0);
assertEq(f(INT32_MIN, INT32_MAX), -1);
assertEq(f(INT32_MIN, INT32_MIN), 1);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return ((i>>>0)/(j>>>0))|0 } return f"));
assertEq(f(4,2), 2);
assertEq(f(3,2), 1);
assertEq(f(3,-2), 0);
assertEq(f(-3,-2), 0);
assertEq(f(0, -1), 0);
assertEq(f(0, INT32_MAX), 0);
assertEq(f(0, INT32_MIN), 0);
assertEq(f(0, UINT32_MAX), 0);
assertEq(f(INT32_MAX, 0), 0);
assertEq(f(INT32_MIN, 0), 0);
assertEq(f(UINT32_MAX, 0), 0);
assertEq(f(-1, INT32_MAX), 2);
assertEq(f(-1, INT32_MIN), 1);
assertEq(f(-1, UINT32_MAX), 1);
assertEq(f(INT32_MAX, -1), 0);
assertEq(f(INT32_MIN, -1), 0);
assertEq(f(UINT32_MAX, -1), 1);
assertEq(f(INT32_MAX, INT32_MAX), 1);
assertEq(f(INT32_MAX, INT32_MIN), 0);
assertEq(f(UINT32_MAX, INT32_MAX), 2);
assertEq(f(INT32_MAX, UINT32_MAX), 0);
assertEq(f(UINT32_MAX, UINT32_MAX), 1);
assertEq(f(INT32_MIN, INT32_MAX), 1);
assertEq(f(INT32_MIN, UINT32_MAX), 0);
assertEq(f(INT32_MIN, INT32_MIN), 1);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var k = 0; k = (i|0)%(j|0)|0; return k|0 } return f"));
assertEq(f(4,2), 0);
assertEq(f(3,2), 1);
assertEq(f(3,-2), 1);
assertEq(f(-3,-2), -1);
assertEq(f(0, -1), 0);
assertEq(f(0, INT32_MAX), 0);
assertEq(f(0, INT32_MIN), 0);
assertEq(f(INT32_MAX, 0), 0);
assertEq(f(INT32_MIN, 0), 0);
assertEq(f(-1, INT32_MAX), -1);
assertEq(f(-1, INT32_MIN), -1);
assertEq(f(INT32_MAX, -1), 0);
assertEq(f(INT32_MIN, -1), 0); // !!
assertEq(f(INT32_MAX, INT32_MAX), 0);
assertEq(f(INT32_MAX, INT32_MIN), INT32_MAX);
assertEq(f(INT32_MIN, INT32_MAX), -1);
assertEq(f(INT32_MIN, INT32_MIN), 0);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var k = 0; k = (i|0)%4|0; return k|0 } return f"));
assertEq(f(0), 0);
assertEq(f(-1), -1);
assertEq(f(-3), -3);
assertEq(f(-4), 0);
assertEq(f(INT32_MIN), 0);
assertEq(f(3), 3);
assertEq(f(4), 0);
assertEq(f(INT32_MAX), 3);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var k = 0; k = (i>>>0)%(j>>>0)|0; return k|0 } return f"));
assertEq(f(4,2), 0);
assertEq(f(3,2), 1);
assertEq(f(3,-2), 3);
assertEq(f(-3,-2), -3);
assertEq(f(0, -1), 0);
assertEq(f(0, INT32_MAX), 0);
assertEq(f(0, INT32_MIN), 0);
assertEq(f(0, UINT32_MAX), 0);
assertEq(f(INT32_MAX, 0), 0);
assertEq(f(INT32_MIN, 0), 0);
assertEq(f(UINT32_MAX, 0), 0);
assertEq(f(-1, INT32_MAX), 1);
assertEq(f(-1, INT32_MIN), INT32_MAX);
assertEq(f(-1, UINT32_MAX), 0);
assertEq(f(INT32_MAX, -1), INT32_MAX);
assertEq(f(INT32_MIN, -1), INT32_MIN);
assertEq(f(UINT32_MAX, -1), 0);
assertEq(f(INT32_MAX, INT32_MAX), 0);
assertEq(f(INT32_MAX, INT32_MIN), INT32_MAX);
assertEq(f(UINT32_MAX, INT32_MAX), 1);
assertEq(f(INT32_MAX, UINT32_MAX), INT32_MAX);
assertEq(f(UINT32_MAX, UINT32_MAX), 0);
assertEq(f(INT32_MIN, INT32_MAX), 1);
assertEq(f(INT32_MIN, UINT32_MAX), INT32_MIN);
assertEq(f(INT32_MIN, INT32_MIN), 0);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { return (4 / 2)|0 } return f"))(), 2);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { return (3 / 2)|0 } return f"))(), 1);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { return (4 % 2)|0 } return f"))(), 0);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { return (3 % 2)|0 } return f"))(), 1);

assertAsmTypeFail(USE_ASM + "function f() { var i=42,j=1.1; return +(i?i:j) } return f");
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=42,j=1.1; return +(i?+(i|0):j) } return f"))(), 42);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { var i=42,j=1; return (i?i:j)|0 } return f"))(), 42);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return ((i|0)>(j|0)?(i+10)|0:(j+100)|0)|0 } return f"));
assertEq(f(2, 4), 104);
assertEq(f(-2, -4), 8);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j,k) { i=i|0;j=j|0;k=k|0; return ((i|0)>(j|0) ? (i|0)>(k|0) ? i : k : (j|0)>(k|0) ? j : k)|0 } return f"));
assertEq(f(1,2,3), 3);
assertEq(f(1,3,2), 3);
assertEq(f(2,1,3), 3);
assertEq(f(2,3,1), 3);
assertEq(f(3,1,2), 3);
assertEq(f(3,2,1), 3);

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; var a=0,b=0; a=i>>>0 < 4294967292; b=(i|0) < -4; return (j ? a : b)|0 } return f"));
assertEq(f(1,true), 1);
assertEq(f(-1,true), 0);
assertEq(f(-5,true), 1);
assertEq(f(1,false), 0);
assertEq(f(-1,false), 0);
assertEq(f(-5,false), 1);

assertAsmTypeFail('glob','imp','b', USE_ASM + HEAP_IMPORTS + "function f() { return (i32[0]+1)|0 } return f");
new Float64Array(BUF_64KB)[0] = 2.3;
assertEq(asmLink(asmCompile('glob','imp','b', USE_ASM + HEAP_IMPORTS + "function f() { return +(f64[0] + 2.0) } return f"), this, null, BUF_64KB)(), 2.3+2);
assertEq(asmLink(asmCompile('glob','imp','b', USE_ASM + HEAP_IMPORTS + "function f() { return +(f64[0] - 2.0) } return f"), this, null, BUF_64KB)(), 2.3-2);
assertEq(asmLink(asmCompile('glob','imp','b', USE_ASM + HEAP_IMPORTS + "function f() { return +(f64[0] * 2.0) } return f"), this, null, BUF_64KB)(), 2.3*2);
assertEq(asmLink(asmCompile('glob','imp','b', USE_ASM + HEAP_IMPORTS + "function f() { return +(f64[0] / 2.0) } return f"), this, null, BUF_64KB)(), 2.3/2);
assertEq(asmLink(asmCompile('glob','imp','b', USE_ASM + HEAP_IMPORTS + "function f() { return +(f64[0] % 2.0) } return f"), this, null, BUF_64KB)(), 2.3%2);
assertEq(asmLink(asmCompile('glob','imp','b', USE_ASM + HEAP_IMPORTS + "function f() { return +-f64[0] } return f"), this, null, BUF_64KB)(), -2.3);
assertEq(asmLink(asmCompile('glob','imp','b', USE_ASM + HEAP_IMPORTS + "var sqrt=glob.Math.sqrt; function f() { return +sqrt(f64[0]) } return f"), this, null, BUF_64KB)(), Math.sqrt(2.3));
new Int32Array(BUF_64KB)[0] = 42;
assertEq(asmLink(asmCompile('glob','imp','b', USE_ASM + HEAP_IMPORTS + "var imul=glob.Math.imul; function f() { return imul(i32[0], 2)|0 } return f"), this, null, BUF_64KB)(), 84);

// beware ye phis of comparisons and integers
var f = asmLink(asmCompile(USE_ASM + "function g(i) { i=i|0; if (i) { i = ((i|0) == 2); } else { i=(i-1)|0 } return i|0; } return g "));
assertEq(f(0), -1);
assertEq(f(1), 0);
assertEq(f(2), 1);
var f = asmLink(asmCompile(USE_ASM + "function g(i) { i=i|0; if (i) { i = !i } else { i=(i-1)|0 } return i|0; } return g "));
assertEq(f(0), -1);
assertEq(f(1), 0);
assertEq(f(2), 0);

// beware ye constant-evaluate of boolean-producing operators
assertEq(asmLink(asmCompile(USE_ASM + "function f() { return (4 | (2 == 2))|0 } return f"))(), 5);
assertEq(asmLink(asmCompile(USE_ASM + "function f() { return (4 | (!2))|0 } return f"))(), 4);

// get that order-of-operations right!
var buf = new ArrayBuffer(4096);
asmLink(asmCompile('glob','imp','buf', USE_ASM + "var i32=new glob.Int32Array(buf); var x=0; function a() { return x|0 } function b() { x=42; return 0 } function f() { i32[((b()|0) & 0x3) >> 2] = a()|0 } return f"), this, null, buf)();
assertEq(new Int32Array(buf)[0], 42);

assertEq(asmLink(asmCompile(USE_ASM + "function f() { var a=0,i=0; for (; ~~i!=4; i=(i+1)|0) { a = (a*5)|0; if (+(a>>>0) != 0.0) return 1; } return 0; } return f"))(), 0)

// Signed integer division by a power of two.
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return ((i|0)/1)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), Math.pow(2,i));
    assertEq(f(Math.pow(2,i)-1), Math.pow(2,i)-1);
    assertEq(f(-Math.pow(2,i)), -Math.pow(2,i));
    assertEq(f(-Math.pow(2,i)-1), -Math.pow(2,i)-1);
}
assertEq(f(INT32_MIN), INT32_MIN);
assertEq(f(INT32_MAX), INT32_MAX);
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return ((i|0)/2)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), (Math.pow(2,i)/2)|0);
    assertEq(f(Math.pow(2,i)-1), ((Math.pow(2,i)-1)/2)|0);
    assertEq(f(-Math.pow(2,i)), (-Math.pow(2,i)/2)|0);
    assertEq(f(-Math.pow(2,i)-1), ((-Math.pow(2,i)-1)/2)|0);
}
assertEq(f(INT32_MIN), (INT32_MIN/2)|0);
assertEq(f(INT32_MAX), (INT32_MAX/2)|0);
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return ((i|0)/4)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), (Math.pow(2,i)/4)|0);
    assertEq(f(Math.pow(2,i)-1), ((Math.pow(2,i)-1)/4)|0);
    assertEq(f(-Math.pow(2,i)), (-Math.pow(2,i)/4)|0);
    assertEq(f(-Math.pow(2,i)-1), ((-Math.pow(2,i)-1)/4)|0);
}
assertEq(f(INT32_MIN), (INT32_MIN/4)|0);
assertEq(f(INT32_MAX), (INT32_MAX/4)|0);
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return ((i|0)/1073741824)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), (Math.pow(2,i)/Math.pow(2,30))|0);
    assertEq(f(Math.pow(2,i)-1), ((Math.pow(2,i)-1)/Math.pow(2,30))|0);
    assertEq(f(-Math.pow(2,i)), (-Math.pow(2,i)/Math.pow(2,30))|0);
    assertEq(f(-Math.pow(2,i)-1), ((-Math.pow(2,i)-1)/Math.pow(2,30))|0);
}
assertEq(f(INT32_MIN), (INT32_MIN/Math.pow(2,30))|0);
assertEq(f(INT32_MAX), (INT32_MAX/Math.pow(2,30))|0);
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return ((((i|0)/1)|0)+i)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), (Math.pow(2,i) * 2)|0);
    assertEq(f(Math.pow(2,i) - 1), ((Math.pow(2,i) - 1) * 2)|0);
    assertEq(f(-Math.pow(2,i)), (-Math.pow(2,i) * 2)|0);
    assertEq(f(-Math.pow(2,i) - 1), ((-Math.pow(2,i) - 1) * 2)|0);
}
assertEq(f(INT32_MIN), (INT32_MIN * 2)|0);
assertEq(f(INT32_MAX), (INT32_MAX * 2)|0);

// Signed integer division by a power of two - with a non-negative numerator!
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; i=(i&2147483647)|0; return ((i|0)/1)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), Math.pow(2,i));
    assertEq(f(Math.pow(2,i+1)-1), Math.pow(2,i+1)-1);
}
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; i=(i&2147483647)|0; return ((i|0)/2)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), (Math.pow(2,i)/2)|0);
    assertEq(f(Math.pow(2,i+1)-1), ((Math.pow(2,i+1)-1)/2)|0);
}
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; i=(i&2147483647)|0; return ((i|0)/4)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), (Math.pow(2,i)/4)|0);
    assertEq(f(Math.pow(2,i+1)-1), ((Math.pow(2,i+1)-1)/4)|0);
}
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; i=(i&2147483647)|0; return ((i|0)/1073741824)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), (Math.pow(2,i)/Math.pow(2,30))|0);
    assertEq(f(Math.pow(2,i+1)-1), ((Math.pow(2,i+1)-1)/Math.pow(2,30))|0);
}
var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; i=(i&2147483647)|0; return ((((i|0)/1)|0)+i)|0; } return f;"));
for (let i = 0; i < 31; i++) {
    assertEq(f(Math.pow(2,i)), (Math.pow(2,i) * 2)|0);
    assertEq(f(Math.pow(2,i+1) - 1), ((Math.pow(2,i+1) - 1) * 2)|0);
}
