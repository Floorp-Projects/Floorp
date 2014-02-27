load(libdir + "asm.js");
const TO_FLOAT32 = "var toF = glob.Math.fround;";
const HEAP32 = "var f32 = new glob.Float32Array(heap);";
var heap = new ArrayBuffer(4096);

// Module linking
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() {} return f"), null);
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() {} return f"), {fround: Math.fround});
assertAsmLinkFail(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() {} return f"), {Math: {fround: Math.imul}});
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() {} return f"), {Math:{fround: Math.fround}})(), undefined);

// Argument coercions
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = unknown(x); } return f");
assertAsmTypeFail('glob', USE_ASM + "function f(i) { i = toF(i); } return f");
assertAsmTypeFail('glob', USE_ASM + "var cos = glob.Math.cos; function f(x) { x = cos(x); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = toF(); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = toF(x, x); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = toF('hi'); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = toF(loat); } return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(i) { i = toF(i); } return f"), this)(), undefined);

// Local variables declarations
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = unknown(); } return f");
assertAsmTypeFail('glob', USE_ASM + "var cos = glob.Math.cos; function f() { var i = cos(); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(x, x); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF('hi'); } return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(5); } return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(5.); } return f"), this)(), undefined);

// Return values
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(4, 4); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF({}); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(x); } return f");

assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(42); } return f"), this)(), 42);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(0.); } return f"), this)(), 0);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(-0.); } return f"), this)(), -0);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var inf = glob.Infinity; function f() { return toF(inf); } return f"), this)(), Infinity);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(13.37); } return f"), this)(), Math.fround(13.37));
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return +toF(4.); } return f"), this)(), 4);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return +~~toF(4.5); } return f"), this)(), 4);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(4.5) | 0; } return f"), this)(), 4);

// Assign values
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(5.); i = 5; return toF(i); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(5.); i = 6.; return toF(i); } return f");

assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(5.); return toF(i); } return f"), this)(), 5);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(5.); i = toF(42); return toF(i); } return f"), this)(), 42);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(5.); i = toF(6.); return toF(i); } return f"), this)(), 6);

assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { var i = toF(5.); f32[0] = toF(6.); i = f32[0]; return toF(i); } return f");
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { var i = toF(5.); f32[0] = toF(6.); i = toF(f32[0]); return toF(i); } return f"), this, null, heap)(), 6);

// Special array assignments (the other ones are tested in testHeapAccess)
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { var i = 5.; f32[0] = i; return toF(f32[0]); } return f"), this, null, heap)(), 5);
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "var f64 = new glob.Float64Array(heap); function f() { var i = toF(5.); f64[0] = i; return +f64[0]; } return f"), this, null, heap)(), 5);

var HEAP64 = "var f64 = new glob.Float64Array(heap);"
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 1.5; return +f32[0]; } return f"), this, null, heap)(), 1.5);
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + HEAP64 + "function f() { f64[0] = 1.5; return toF(f64[0]); } return f"), this, null, heap)(), 1.5);

// Coercions
// -> from Float32
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { var i = toF(5.); var n = 0; n = i | 0; return n | 0; } return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = toF(x); var n = 0.; n = +x; return +n; } return f"), this)(16.64), Math.fround(16.64));
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = toF(x); var n = 0; n = ~~x; return n | 0; } return f"), this)(16.64), 16);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = toF(x); var n = 0; n = ~~x >>> 0; return n | 0; } return f"), this)(16.64), 16);

// -> from float?
function makeCoercion(coercionFunc) {
    return USE_ASM + HEAP32 + TO_FLOAT32 + "function f(x) { x = toF(x); f32[0] = x; return " + coercionFunc('f32[0]') + " } return f";
}
assertAsmTypeFail('glob', 'ffi', 'heap', makeCoercion(x => x + '|0'));
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', makeCoercion(x => '+' + x)), this, null, heap)(16.64), Math.fround(16.64));
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', makeCoercion(x => 'toF(' + x + ')')), this, null, heap)(16.64), Math.fround(16.64));
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', makeCoercion(x => '~~+' + x + '|0')), this, null, heap)(16.64), 16);
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', makeCoercion(x => '~~toF(' + x + ')|0')), this, null, heap)(16.64), 16);

// -> to Float32
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = x|0; return toF(~~x); } return f"), this)(23), 23);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = x|0; return toF(x >> 0); } return f"), this)(23), 23);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = +x; return toF(x); } return f"), this)(13.37), Math.fround(13.37));

UINT32_MAX = Math.pow(2, 32)-1;
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = x|0; return toF(x >>> 0); } return f"), this)(-1), Math.fround(UINT32_MAX));
var tof = asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = x|0; return toF(x>>>0) } return f"), this);
for (x of [0, 1, 10, 64, 1025, 65000, Math.pow(2,30), Math.pow(2,31), Math.pow(2,32)-2, Math.pow(2,32)-1])
    assertEq(tof(x), Math.fround(x));

// Global variables imports
assertAsmTypeFail('glob', USE_ASM + "var x = toF(); function f() {} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var x = some(3); function f() {} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var x = toF(); function f() {} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var x = toF(3, 4); function f() {} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var x = toF({x: 3}); function f() {} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var x = toF(true); function f() {} return f");
assertAsmTypeFail('glob', USE_ASM + "var x = toF(3);" + TO_FLOAT32 + "function f() {} return f");

assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var x = toF(3.5); function f() {} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var x = toF(3); function f() {} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', 'ffi', USE_ASM + TO_FLOAT32 + "var x = toF(ffi.x); function f() {} return f"), this, {x:3})(), undefined);
assertEq(asmLink(asmCompile('glob', 'ffi', USE_ASM + TO_FLOAT32 + "var x = toF(ffi.x); function f() {} return f"), this, {x:3.5})(), undefined);

// Global variables uses
values = [2.01, 13.37, -3.141592653]
specials = [NaN, Infinity]

for (v of values) {
    assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var x = toF(" + v + "); function f() {return toF(x);} return f"), this)(), Math.fround(v));
    assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "const x = toF(" + v + "); function f() {return toF(x);} return f"), this)(), Math.fround(v));
    assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var x = toF(0.); function f() {x = toF(" + v + "); return toF(x);} return f"), this)(), Math.fround(v));
    assertEq(asmLink(asmCompile('glob', 'ffi', USE_ASM + TO_FLOAT32 + "var x = toF(ffi.x); function f() {return toF(x);} return f"), this, {x:v})(), Math.fround(v));
}

for (v of specials) {
    assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var special = glob." + v + "; var g=toF(0.); function f() {g=toF(special); return toF(g);} return f"), this)(), Math.fround(v));
}

// Math builtins
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var imul = glob.Math.imul; function f() {var x = toF(1.5), y = toF(2.4); return imul(x, y) | 0;} return f");

assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var abs = glob.Math.abs; function f() {var x = toF(1.5); return +abs(x);} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var abs = glob.Math.abs; function f() {var x = toF(1.5); return abs(x) | 0;} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var abs = glob.Math.abs; function f() {var x = toF(1.5); return toF(abs(x))} return f"), this)(), Math.fround(1.5));
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var abs = glob.Math.abs; function f() {var x = toF(-1.5); return toF(abs(x))} return f"), this)(), Math.fround(1.5));

assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var sqrt = glob.Math.sqrt; function f() {var x = toF(1.5); return +sqrt(x);} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var sqrt = glob.Math.sqrt; function f() {var x = toF(1.5); return sqrt(x) | 0;} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var sqrt = glob.Math.sqrt; function f() {var x = toF(2.25); return toF(sqrt(x))} return f"), this)(), Math.fround(1.5));
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var sqrt = glob.Math.sqrt; function f() {var x = toF(-1.); return toF(sqrt(x))} return f"), this)(), NaN);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var sqrt = glob.Math.sqrt; var inf = glob.Infinity; function f() {var x = toF(0.); x = toF(inf); return toF(sqrt(x))} return f"), this)(), Infinity);

// float?s
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + HEAP32 + TO_FLOAT32 + "var sqrt = glob.Math.sqrt; function f(x) { x = toF(x); f32[0] = x; return toF(sqrt(f32[0])) } return f"), this, null, heap)(64), Math.fround(8));

// The only other Math functions that can receive float32 as arguments and that strictly commute are floor and ceil (see
// also bug 969203).
var floorModule = asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.floor; function f(x) {x = toF(x); return toF(g(x))} return f"), this);
for (v of [-10.5, -1.2345, -1, 0, 1, 3.141592653, 13.37, Math.Infinity, NaN])
    assertEq(floorModule(v), Math.fround(Math.floor(Math.fround(v))));
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + HEAP32 + TO_FLOAT32 + "var floor = glob.Math.floor; function f(x) { x = toF(x); f32[0] = x; return toF(floor(f32[0])) } return f"), this, null, heap)(13.37), 13);

var ceilModule = asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.ceil; function f(x) {x = toF(x); return toF(g(x))} return f"), this);
for (v of [-10.5, -1.2345, -1, 0, 1, 3.141592653, 13.37, Math.Infinity, NaN])
    assertEq(ceilModule(v), Math.fround(Math.ceil(Math.fround(v))));
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + HEAP32 + TO_FLOAT32 + "var ceil = glob.Math.ceil; function f(x) { x = toF(x); f32[0] = x; return toF(ceil(f32[0])) } return f"), this, null, heap)(13.37), 14);

assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.cos; function f(x) {x = toF(x); return toF(g(x))} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.cos; function f(x) {x = +x; return toF(g(+x))} return f");

assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.cos; function f(x) {x = toF(x); return +(g(x))} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.cos; function f(x) {x = +x; return toF(g(x))} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.cos; function f(x) {x = x|0; return toF(g(x))} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.cos; function f(x) {x = toF(x); return g(x) | 0} return f");

assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + HEAP32 + TO_FLOAT32 + "var g = glob.Math.cos; function f(x) { x = toF(x); return toF(+g(+x)) } return f"), this, null, heap)(3.14159265358), -1);

// Math functions with arity of two are not specialized for floats, so we shouldn't feed them with floats arguments or
// return type
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.pow; function f(x) {x = toF(x); return toF(g(x, 2.))} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.pow; function f(x) {x = toF(x); return +g(x, 2.)} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.pow; function f(x) {x = toF(x); return toF(g(+x, 2.))} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.pow; function f(x) {x = toF(x); return +g(+x, 2.)} return f"), this)(3), 9);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var g = glob.Math.pow; function f(x) {x = toF(x); return toF(+g(+x, 2.))} return f"), this)(3), 9);

// Other function calls
// -> Signature comparisons
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function g(x){x=toF(x); return toF(x);} function f() {var x=toF(4.); var y=toF(0.); var z = 0.; y=toF(g(x)); z = +g(x); return toF(z); } return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var sqrt=glob.Math.sqrt; function g(x){x=toF(x); return toF(sqrt(x));} function f() {var x=toF(4.); var y=toF(0.); var z = 0.; y = toF(g(x)); z = +toF(g(x)); return toF(z); } return f"), this)(), 2);

// -> FFI
var FFI = "var ffi = env.ffi;";
assertAsmTypeFail('glob', 'env', USE_ASM + TO_FLOAT32 + FFI + "function f() {var x = toF(3.14); return +ffi(x);} return f");
assertAsmTypeFail('glob', 'env', USE_ASM + TO_FLOAT32 + FFI + "function f() {var x = toF(3.14); return toF(ffi(+x));} return f");

var env = {ffi: function(x) { return x+1; }}; // use registers
assertEq(asmLink(asmCompile('glob', 'env', USE_ASM + TO_FLOAT32 + FFI + "function f(x) {x = toF(x); return toF(+ffi(+x));} return f"), this, env)(5), Math.fround(6));
env = {ffi: function(a,b,c,d,e,f,g,h,i) { return a+b+c+d+e+f+g+h+i; }}; // use stack arguments (> 8 arguments on linux x64)
assertEq(asmLink(asmCompile('glob', 'env', USE_ASM + TO_FLOAT32 + FFI + "function f(x) {x = toF(x); return toF(+ffi(+x, 1., 2., 3., 4., 5., -5., -4., 1.));} return f"), this, env)(5), Math.fround(12));

// -> Internal calls
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function g(x){x=toF(x);return toF(+x + 1.);} function f(x) {x = +x; return toF(g(x));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function g(x){x=toF(x);return toF(+x + 1.);} function f(x) {x = x|0; return toF(g(x));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function g(x){x=toF(x);return toF(+x + 1.);} function f(x) {x = x|0; return toF(g(x));} return f");

assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function g(x){x=toF(x);return toF(+x + 1.);} function f(x) {x = toF(x); return toF(g(x));} return f"), this, env)(5), Math.fround(6));
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function g(x,y){x=toF(x);y=toF(y);return toF(+x + +y);} function f(x) {x = toF(x); return toF(g(x, toF(1.)));} return f"), this, env)(5), Math.fround(6));

// --> internal calls with unused return values
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "var g = 4; function s(x) { x = +x; g = (g + ~~x)|0; return g|0;} function f(x) { x = +x; toF(s(x)); return g|0} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var g = 4; function s(x) { x = +x; g = (g + ~~x)|0; return toF(g|0);} function f(x) { x = +x; toF(s(x)); return g|0} return f"), this)(3), 7);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var g = 4; function s(x) { x = toF(x); g = (g + ~~x)|0; return toF(g|0);} function f(x) { x = toF(x); return (toF(s(x)), g)|0} return f"), this)(3), 7);

// --> coerced calls
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var g = 4; function s(x) { x = toF(x); g = (g + ~~x)|0; return +(g|0);} function f(x) { x = toF(x); return toF(+s(x))} return f"), this)(3), 7);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var g = 4; function s(x) { x = toF(x); g = (g + ~~x)|0; return g|0;} function f(x) { x = toF(x); return toF(s(x)|0)} return f"), this)(3), 7);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "var g = 4; function s(x) { x = toF(x); g = (g + ~~x)|0; return toF(g|0);} function f(x) { x = toF(x); return toF(toF(s(x)))} return f"), this)(3), 7);

// --> test pressure on registers in internal calls when there are |numArgs| arguments
for (numArgs of [5, 9, 17, 33, 65, 129]) {
    let code = (function(n) {
        let args = "", coercions = "", sum = "", call="x";
        for (let i = 0; i < n; i++) {
            let name = 'a' + i;
            args += name + ((i == n-1)?'':',');
            coercions += name + '=toF(' + name + ');';
            sum += ((i>0)?'+':'') + ' +' + name;
            call += (i==0)?'':',toF(' + i + '.)'
        }
        return "function g(" + args + "){" + coercions + "return toF(" + sum + ");}"
               +"function f(x) { x = toF(x); return toF(g(" + call + "))}";
    })(numArgs);
    assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + code + "return f"), this, env)(5), Math.fround(5 + numArgs * (numArgs-1) / 2));
}

// -> Pointer calls
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function a(x){x=toF(x);return toF(+x + .5);} function b(x){x=toF(x);return toF(+x - .5);} function f(x, n) {x=+x;n=n|0;return toF(t[n&1](x));} var t=[a,b]; return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function a(x){x=toF(x);return toF(+x + .5);} function b(x){x=toF(x);return toF(+x - .5);} function f(x, n) {x=x|0;n=n|0;return toF(t[n&1](x));} var t=[a,b]; return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function a(x){x=toF(x);return toF(+x + .5);} function b(x){x=toF(x);return toF(+x - .5);} function f(x, n) {x=toF(x);n=n|0;return t[n&1](x)|0;} var t=[a,b]; return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function a(x){x=toF(x);return toF(+x + .5);} function b(x){x=toF(x);return toF(+x - .5);} function f(x, n) {x=toF(x);n=n|0;return +t[n&1](x);} var t=[a,b]; return f");

code = asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function a(x){x=toF(x);return toF(+x + .5);} function b(x){x=toF(x);return toF(+x - .5);}"
               + "function f(x, n) {x=toF(x);n=n|0;return toF(t[n&1](x));} var t=[a,b]; return f"), this);
assertEq(code(0, 0), .5);
assertEq(code(0, 1), -.5);
assertEq(code(13.37, 0), Math.fround(13.37 + .5));
assertEq(code(13.37, 1), Math.fround(13.37 - .5));

// Arithmetic operations
// -> mul
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(3 * toF(4.));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(3. * toF(4.)); } return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return +(toF(3.) * toF(4.));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(3) * toF(4) * toF(5));} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(3.) * toF(4.)); } return f"), this)(), 12);

assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(3 * f32[0]);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(3. * f32[0]);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return +(toF(3.) * f32[0]);} return f");
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(toF(3.) * f32[0]);} return f"), this, null, heap)(), 12);

var mul = asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x=toF(x); return toF(x * toF(4.));} return f"), this);
assertEq(mul(Infinity), Infinity);
assertEq(mul(NaN), NaN);
assertEq(mul(0), 0);
assertEq(mul(1), 4);
assertEq(mul(0.33), Math.fround(0.33 * 4));

// -> add
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(3 + toF(4.));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(3.5 + toF(4.));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return +(toF(3.5) + toF(4.));} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(3.5) + toF(4.)); } return f"), this)(), 7.5);
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(3.5) + toF(4.) + toF(4.5));} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(toF(3.5) + toF(4.)) + toF(4.5)); } return f"), this)(), 12);
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(toF(3) + f32[0]);} return f"), this, null, heap)(), 7);

// --> no additions with float? or floatish
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(3 + f32[0]);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(3. + f32[0]);} return f");

// -> sub
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(3 - toF(4.));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(3.5 - toF(4.));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return +(toF(3.5) - toF(4.));} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(3.5) - toF(4.)); } return f"), this)(), -.5);
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(3.5) - toF(4.) - toF(4.5));} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(toF(3.5) - toF(4.)) - toF(4.5)); } return f"), this)(), -5);

assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(3.5) + toF(4.) - toF(4.5));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(3.5) - toF(4.) + toF(4.5));} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(toF(3.5) + toF(4.)) - toF(4.5)); } return f"), this)(), 3);
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(toF(3.5) - toF(4.)) + toF(4.5)); } return f"), this)(), 4);

assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(3 - f32[0]);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(3. - f32[0]);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return +(toF(3) - f32[0]);} return f");
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(toF(3) - f32[0]);} return f"), this, null, heap)(), -1);

// -> div
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(3 / toF(4.));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(3.5 / toF(4.));} return f");
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return +(toF(3.5) / toF(4.));} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(12.) / toF(4.)); } return f"), this)(), 3);

assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(3 / f32[0]);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return toF(3. / f32[0]);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 4.; return +(toF(3) / f32[0]);} return f");
assertEq(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { f32[0] = 2.; return toF(toF(4) / f32[0]);} return f"), this, null, heap)(), 2);

// -> mod
assertAsmTypeFail('glob', USE_ASM + TO_FLOAT32 + "function f() { return toF(toF(3.5) % toF(4.));} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { return toF(f32[0] % toF(4.));} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { return toF(toF(3.5) % f32[0]);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { return toF(f32[1] % f32[0]);} return f");

// Comparisons
for (op of ['==', '!=', '<', '>', '<=', '>=']) {
    let code = asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = toF(x); if( x " + op + " toF(3.) ) return 1; else return 0; return -1; } return f"), this);
    let ternary = asmLink(asmCompile('glob', USE_ASM + TO_FLOAT32 + "function f(x) { x = toF(x); return ((x " + op + " toF(3.)) ? 1 : 0)|0 } return f"), this);
    for (v of [-5, 0, 2.5, 3, 13.37, NaN, Infinity]) {
        let expected = eval("("+ v + " " + op + " 3)|0");
        assertEq(code(v) | 0, expected);
        assertEq(ternary(v) | 0, expected);
    }

    assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { if( f32[0] " + op + " toF(3.) ) return 1; else return 0; return -1; } return f");
    assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + TO_FLOAT32 + HEAP32 + "function f() { if( (toF(1.) + toF(2.)) " + op + " toF(3.) ) return 1; else return 0; return -1; } return f");
}
