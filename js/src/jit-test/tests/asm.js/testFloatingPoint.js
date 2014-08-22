load(libdir + "asm.js");

assertEq(asmLink(asmCompile(USE_ASM + "function f() { return 1.1 } return f"))(), 1.1);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; return +(+(i|0) + .1) } return f"))(1), 1.1);
assertEq(asmLink(asmCompile(USE_ASM + "function f(d) { d=+d; return +d } return f"))(1.1), 1.1);
assertEq(asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return +(d+e) } return f"))(1.0, .1), 1.1);
assertEq(asmLink(asmCompile(USE_ASM + "function f(i,e) { i=i|0;e=+e; return +(+~~i+e) } return f"))(1, .1), 1.1);
assertEq(asmLink(asmCompile(USE_ASM + "function f(d,i) { d=+d;i=i|0; return +(d + +(i|0)) } return f"))(.1, 1), 1.1);
assertEq(asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return +(d-e) } return f"))(1.1, .8), (1.1-.8));
assertEq(asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return +(d*e) } return f"))(1.1, 2.2), (1.1*2.2));
assertEq(asmLink(asmCompile(USE_ASM + "function g() { var i=2; return (~~(i=(i+1)|0))|0 } return g"))(), 3);

var f = asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return (d<e)|0 } return f"));
assertEq(f(1.1, 2.2), 1);
assertEq(f(1.1, 1.1), 0);
assertEq(f(2.1, 1.1), 0);

var f = asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return (d<=e)|0 } return f"));
assertEq(f(1.1, 2.2), 1);
assertEq(f(1.1, 1.1), 1);
assertEq(f(2.1, 1.1), 0);
assertEq(f(NaN, 1.1), 0);
assertEq(f(1.1, NaN), 0);
assertEq(f(NaN, NaN), 0);

var f = asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return (d>e)|0 } return f"));
assertEq(f(2.1, 1.1), 1);
assertEq(f(1.1, 1.1), 0);
assertEq(f(1.1, 2.1), 0);
assertEq(f(NaN, 1.1), 0);
assertEq(f(1.1, NaN), 0);
assertEq(f(NaN, NaN), 0);

var f = asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return (d>=e)|0 } return f"));
assertEq(f(2.1, 1.1), 1);
assertEq(f(1.0, 1.1), 0);
assertEq(f(1.1, 2.1), 0);
assertEq(f(NaN, 1.1), 0);
assertEq(f(1.1, NaN), 0);
assertEq(f(NaN, NaN), 0);

var f = asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return (d==e)|0 } return f"));
assertEq(f(2.1, 1.1), 0);
assertEq(f(1.1, 1.1), 1);
assertEq(f(1.1, 2.1), 0);
assertEq(f(NaN, 1.1), 0);
assertEq(f(1.1, NaN), 0);
assertEq(f(NaN, NaN), 0);

var f = asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return (d!=e)|0 } return f"));
assertEq(f(2.1, 1.1), 1);
assertEq(f(1.1, 1.1), 0);
assertEq(f(1.1, 2.1), 1);
assertEq(f(NaN, 1.1), 1);
assertEq(f(1.1, NaN), 1);
assertEq(f(NaN, NaN), 1);

var f = asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return +(d/e) } return f"));
assertEq(f(1.1, .1), (1.1/.1));
assertEq(f(1.1, 0),  (1.1/0));
assertEq(f(1.1, -0), (1.1/-0));

var f = asmLink(asmCompile(USE_ASM + "function f(d,e) { d=+d;e=+e; return +(d%e) } return f"));
assertEq(f(1.1, .1), (1.1%.1));
assertEq(f(1.1, 0),  (1.1%0));
assertEq(f(1.1, -0), (1.1%-0));

var f = asmLink(asmCompile(USE_ASM + "function f(d) { d=+d; var i = 0; i = ~~d; return i|0 } return f"));
assertEq(f(1.0), 1);
assertEq(f(1.9), 1);
assertEq(f(1.9999), 1);
assertEq(f(2.0), 2);
assertEq(f(Math.pow(2,40)), ~~Math.pow(2,40));
assertEq(f(-Math.pow(2,40)), ~~-Math.pow(2,40));
assertEq(f(4000000000), ~~4000000000);
assertEq(f(-4000000000), ~~-4000000000);
assertEq(f(NaN), 0);
assertEq(f(Infinity), 0);
assertEq(f(-Infinity), 0);

assertAsmTypeFail(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return +((i|0)/(j|0)) } return f");
assertAsmTypeFail(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return +(i+j) } return f");
assertAsmTypeFail(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return +(i-j) } return f");

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return +(((i|0)/(j|0))|0) } return f"));
assertEq(f(1,0), 0);
assertEq(f(-Math.pow(2,31),-1), -Math.pow(2,31));

var f = asmLink(asmCompile(USE_ASM + "function f(i,j) { i=i|0;j=j|0; return +(((i|0)%(j|0))|0) } return f"));
assertEq(f(1,0), 0);
assertEq(f(-Math.pow(2,31),-1), 0);

var {f,g} = asmLink(asmCompile(USE_ASM + "function f() { return 3.5 } function g(d) { d=+d; return +(d+3.5) } return {f:f,g:g}"));
assertEq(f(), 3.5);
assertEq(g(1), 1+3.5);

var buf = new ArrayBuffer(BUF_MIN);
var f64 = new Float64Array(buf);
var i32 = new Int32Array(buf);
var u32 = new Uint32Array(buf);
var f = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function f() { return +f64[0] } return f'), this, null, buf);
f64[0] = 0;
assertEq(f(), 0);
f64[0] = -1;
assertEq(f(), -1);
f64[0] = 1;
assertEq(f(), 1);
f64[0] = Infinity;
assertEq(f(), Infinity);
f64[0] = -Infinity;
assertEq(f(), -Infinity);

function ffi(d) { str = String(d) }
var g = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'var ffi=imp.ffi; function g() { ffi(+f64[0]) } return g'), this, {ffi:ffi}, buf);
var h = asmLink(asmCompile('glob', 'imp', 'b', USE_ASM + HEAP_IMPORTS + 'function g() { return +(+f64[0] < 0.0 ? -+f64[0] : +f64[0]) } return g'), this, null, buf)

function ffi1() { return 2.6 }
assertEq(asmLink(asmCompile('glob', 'imp', USE_ASM + "var ffi1=imp.ffi1; function g() { var i=0,j=0.0; i=ffi1()|0; j=+ffi1(); return +(+(i|0)+j) } return g"), null, {ffi1:ffi1})(), 2+2.6);

// that sounds dangerous!
var a = [0,1,0xffff0000,0x7fff0000,0xfff80000,0x7ff80000,0xfffc0000,0x7ffc0000,0xffffffff,0x0000ffff,0x00008fff7];
for (i of a) {
    for (j of a) {
        u32[0] = i;
        u32[1] = j;

        assertEq(f(), f64[0]);

        g();
        assertEq(str, String(f64[0]));

        assertEq(h(), Math.abs(f64[0]));
    }
}
