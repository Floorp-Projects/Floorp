load(libdir + "asm.js");
load(libdir + "simd.js");
load(libdir + "asserts.js");
var heap = new ArrayBuffer(0x10000);

// Avoid pathological --ion-eager compile times due to bails in loops
setJitCompilerOption('ion.warmup.trigger', 1000000);

// Set to true to see more JS debugging spew
const DEBUG = false;

if (!isSimdAvailable() || typeof SIMD === 'undefined' || !isAsmJSCompilationAvailable()) {
    DEBUG && print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

const I32 = 'var i4 = glob.SIMD.Int32x4;'
const CI32 = 'var ci4 = i4.check;'
const I32A = 'var i4a = i4.add;'
const I32S = 'var i4s = i4.sub;'
const I32M = 'var i4m = i4.mul;'
const I32U32 = 'var i4u4 = i4.fromUint32x4Bits;'

const U32 = 'var u4 = glob.SIMD.Uint32x4;'
const CU32 = 'var cu4 = u4.check;'
const U32A = 'var u4a = u4.add;'
const U32S = 'var u4s = u4.sub;'
const U32M = 'var u4m = u4.mul;'
const U32I32 = 'var u4i4 = u4.fromInt32x4Bits;'

const F32 = 'var f4 = glob.SIMD.Float32x4;'
const CF32 = 'var cf4 = f4.check;'
const F32A = 'var f4a = f4.add;'
const F32S = 'var f4s = f4.sub;'
const F32M = 'var f4m = f4.mul;'
const F32D = 'var f4d = f4.div;'
const FROUND = 'var f32=glob.Math.fround;'
const B32 = 'var b4 = glob.SIMD.Bool32x4;'
const CB32 = 'var cb4 = b4.check;'

const EXTI4 = 'var e = i4.extractLane;'
const EXTU4 = 'var e = u4.extractLane;'
const EXTF4 = 'var e = f4.extractLane;'
const EXTB4 = 'var e = b4.extractLane;'

// anyTrue / allTrue on boolean vectors.
const ANYB4 = 'var anyt=b4.anyTrue;'
const ALLB4 = 'var allt=b4.allTrue;'

const INT32_MAX = Math.pow(2, 31) - 1;
const INT32_MIN = INT32_MAX + 1 | 0;
const UINT32_MAX = Math.pow(2, 32) - 1;

const assertEqFFI = {assertEq:assertEq};

function CheckI4(header, code, expected) {
    // code needs to contain a local called x
    header = USE_ASM + I32 + CI32 + EXTI4 + F32 + header;
    var observed = asmLink(asmCompile('glob', header + ';function f() {' + code + ';return ci4(x)} return f'), this)();
    assertEqX4(observed, expected);
}

function CheckU4(header, code, expected) {
    // code needs to contain a local called x.
    header = USE_ASM + U32 + CU32 + EXTU4 + I32 + CI32 + I32U32 + header;
    var observed = asmLink(asmCompile('glob', header + ';function f() {' + code + ';return ci4(i4u4(x))} return f'), this)();
    // We can't return an unsigned SIMD type. Return Int32x4, convert to unsigned here.
    observed = SIMD.Uint32x4.fromInt32x4Bits(observed)
    assertEqX4(observed, expected);
}

function CheckF4(header, code, expected) {
    // code needs to contain a local called x
    header = USE_ASM + F32 + CF32 + EXTF4 + header;
    var observed = asmLink(asmCompile('glob', header + ';function f() {' + code + ';return cf4(x)} return f'), this)();
    assertEqX4(observed, expected.map(Math.fround));
}

function CheckB4(header, code, expected) {
    // code needs to contain a local called x
    header = USE_ASM + B32 + CB32 + header;
    var observed = asmLink(asmCompile('glob', header + ';function f() {' + code + ';return cb4(x)} return f'), this)();
    assertEqX4(observed, expected);
}

try {

// 1. Constructors

// 1.1 Compilation
assertAsmTypeFail('glob', USE_ASM + "var i4 = Int32x4               ; return {}") ;
assertAsmTypeFail('glob', USE_ASM + "var i4 = glob.Int32x4          ; return {}") ;
assertAsmTypeFail('glob', USE_ASM + "var i4 = glob.globglob.Int32x4 ; return {}") ;
assertAsmTypeFail('glob', USE_ASM + "var i4 = glob.Math.Int32x4     ; return {}") ;
assertAsmTypeFail('glob', USE_ASM + "var herd = glob.SIMD.ponyX4    ; return {}") ;

// 1.2 Linking
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: 42});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: Math.fround});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {Int32x4: 42}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {Int32x4: Math.fround}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {Int32x4: new Array}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {Int32x4: SIMD.Float32x4}});

var [Type, int32] = [TypedObject.StructType, TypedObject.int32];
var MyStruct = new Type({'x': int32, 'y': int32, 'z': int32, 'w': int32});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {Int32x4: MyStruct}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + I32 + "return {}"), {SIMD: {Int32x4: new MyStruct}});

assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {} return f"), {SIMD:{Int32x4: SIMD.Int32x4}})(), undefined);

assertAsmLinkFail(asmCompile('glob', USE_ASM + F32 + "return {}"), {SIMD: {Float32x4: 42}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + F32 + "return {}"), {SIMD: {Float32x4: Math.fround}});
assertAsmLinkFail(asmCompile('glob', USE_ASM + F32 + "return {}"), {SIMD: {Float32x4: new Array}});
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {} return f"), {SIMD:{Float32x4: SIMD.Float32x4}})(), undefined);

// 1.3 Correctness
// 1.3.1 Local variables declarations
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=Int32x4(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1, 2);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1, 2, 3);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1, 2, 3, 4.0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1, 2.0, 3, 4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4a(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3,2+2|0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3," + (INT32_MIN - 1) + ");} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {var x=i4(i4(1,2,3,4));} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3,4);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {var x=i4(1,2,3," + (INT32_MAX + 1) + ");} return f"), this)(), undefined);

assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4();} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1.,2.,3.);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1.,2.,f32(3.),4.);} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {var x=f4(1.,2.,3.,4.);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3," + (INT32_MIN - 1) + ");} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3," + (INT32_MAX + 1) + ");} return f"), this)(), undefined);

// Places where NumLit can creep in
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(i) {i=i|0; var z=0; switch(i|0) {case i4(1,2,3,4): z=1; break; default: z=2; break;}} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(i) {i=i|0; var z=0; return i * i4(1,2,3,4) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(i) {var x=i4(1,2,3,i4(4,5,6,7))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "function f(i) {var x=i4(1,2,3,f4(4,5,6,7))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "function f(i) {var x=f4(1,2,3,i4(4,5,6,7))} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {return +i4(1,2,3,4)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f() {return i4(1,2,3,4)|0} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + FROUND + "function f() {return f32(i4(1,2,3,4))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + "function f() {return cf4(i4(1,2,3,4))} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {return +f4(1,2,3,4)} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {return f4(1,2,3,4)|0} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + FROUND + "function f() {return f32(f4(1,2,3,4))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + F32 + "function f() {return ci4(f4(1,2,3,4))} return f");

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {return i4(1,2,3,4);} return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f() {return ci4(i4(1,2,3,4));} return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {return f4(1,2,3,4);} return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f() {return cf4(f4(1,2,3,4));} return f"), this)(), [1, 2, 3, 4]);

assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + "function f() {i4(1,2,3,4);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + "function f() {f4(1,2,3,4);} return f"), this)(), undefined);

// Int32x4 ctor should accept int?
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + I32 + CI32 + "var i32=new glob.Int32Array(heap); function f(i) {i=i|0; return ci4(i4(i32[i>>2], 2, 3, 4))} return f"), this, {}, new ArrayBuffer(0x10000))(0x20000), [0, 2, 3, 4]);
// Float32x4 ctor should accept floatish (i.e. float || float? || floatish) and doublit
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + F32 + CF32 + FROUND + "var h=new glob.Float32Array(heap); function f(i) {i=i|0; return cf4(f4(h[i>>2], f32(2), f32(3), f32(4)))} return f"), this, {}, new ArrayBuffer(0x10000))(0x20000), [NaN, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + FROUND + "function f(i) {i=i|0; return cf4(f4(f32(1) + f32(2), f32(2), f32(3), f32(4)))} return f"), this, {}, new ArrayBuffer(0x10000))(0x20000), [3, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + FROUND + "function f(i) {i=i|0; return cf4(f4(f32(1) + f32(2), 2.0, 3.0, 4.0))} return f"), this, {}, new ArrayBuffer(0x10000))(0x20000), [3, 2, 3, 4]);
// Bool32x4 ctor should accept int?
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + B32 + CB32 + "var i32=new glob.Int32Array(heap); function f(i) {i=i|0; return cb4(b4(i32[i>>2], 2, 0, 4))} return f"), this, {}, new ArrayBuffer(0x10000))(0x20000), [false, true, false, true]);

// 1.3.2 Getters - Reading values out of lanes
assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=1; return e(x,1) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=1; return e(x + x, 1) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=1.; return e(x, 1) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + EXTF4 + "var f32=glob.Math.fround;" + I32 + "function f() {var x=f32(1); return e(x, 1) | 0;} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=i4(1,2,3,4); return x.length|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=e(i4(1,2,3,4),1); return x|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=i4(1,2,3,4); return (e(x,0) > (1>>>0)) | 0;} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=i4(1,2,3,4); return e(x,-1) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=i4(1,2,3,4); return e(x,4) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=i4(1,2,3,4); return e(x,.5) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=i4(1,2,3,4); return e(x,x) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + EXTF4 + "function f() {var x=i4(1,2,3,4); return e(x,0) | 0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + EXTI4 + "function f() {var x=i4(1,2,3,4); var i=0; return e(x,i) | 0;} return f");

// The signMask property is no longer supported. Replaced by allTrue / anyTrue.
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=42; return x.signMask;} return f");
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=42.; return x.signMask;} return f");
assertAsmTypeFail('glob', USE_ASM + FROUND + "function f() {var x=f32(42.); return x.signMask;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + 'function f() { var x=i4(1,2,3,4); return x.signMask | 0 } return f');
assertAsmTypeFail('glob', USE_ASM + U32 + 'function f() { var x=u4(1,2,3,4); return x.signMask | 0 } return f');
assertAsmTypeFail('glob', USE_ASM + F32 + FROUND + 'var Infinity = glob.Infinity; function f() { var x=f4(0,0,0,0); x=f4(f32(1), f32(-13.37), f32(42), f32(-Infinity)); return x.signMask | 0 } return f');

// Check lane extraction.
function CheckLanes(innerBody, type, expected) {
    var coerceBefore, coerceAfter, extractLane;

    if (type === SIMD.Int32x4) {
        coerceBefore = '';
        coerceAfter = '|0';
        extractLane = 'ei';
    } else if (type === SIMD.Uint32x4) {
        // Coerce Uint32 lanes to double so they can be legally returned.
        coerceBefore = '+';
        coerceAfter = '';
        extractLane = 'eu';
    } else if (type === SIMD.Float32x4) {
        coerceBefore = '+';
        coerceAfter = '';
        extractLane = 'ef';
        expected = expected.map(Math.fround);
    } else if (type === SIMD.Bool32x4) {
        coerceBefore = '';
        coerceAfter = '|0';
        extractLane = 'eb';
    } else throw "unexpected type in CheckLanes";

    for (var i = 0; i < 4; i++) {
        var lane = i;
        var laneCheckCode = `"use asm";
            var i4=glob.SIMD.Int32x4;
            var u4=glob.SIMD.Uint32x4;
            var f4=glob.SIMD.Float32x4;
            var b4=glob.SIMD.Bool32x4;
            var ei=i4.extractLane;
            var eu=u4.extractLane;
            var ef=f4.extractLane;
            var eb=b4.extractLane;
            function f() {${innerBody}; return ${coerceBefore}${extractLane}(x, ${lane})${coerceAfter} }
            return f;`;
        assertEq(asmLink(asmCompile('glob', laneCheckCode), this)(), expected[i]);
    }
}
function CheckLanesI4(innerBody, expected) { return CheckLanes(innerBody, SIMD.Int32x4, expected); }
function CheckLanesU4(innerBody, expected) { return CheckLanes(innerBody, SIMD.Uint32x4, expected); }
function CheckLanesF4(innerBody, expected) { return CheckLanes(innerBody, SIMD.Float32x4, expected); }
function CheckLanesB4(innerBody, expected) { return CheckLanes(innerBody, SIMD.Bool32x4, expected); }

CheckLanesI4('var x=i4(0,0,0,0);', [0,0,0,0]);
CheckLanesI4('var x=i4(1,2,3,4);', [1,2,3,4]);
CheckLanesI4('var x=i4(' + INT32_MIN + ',2,3,' + INT32_MAX + ')', [INT32_MIN,2,3,INT32_MAX]);
CheckLanesI4('var x=i4(1,2,3,4); var y=i4(5,6,7,8)', [1,2,3,4]);
CheckLanesI4('var a=1; var b=i4(9,8,7,6); var c=13.37; var x=i4(1,2,3,4); var y=i4(5,6,7,8)', [1,2,3,4]);
CheckLanesI4('var y=i4(5,6,7,8); var x=i4(1,2,3,4)', [1,2,3,4]);

CheckLanesU4('var x=u4(0,0,0,0);', [0,0,0,0]);
CheckLanesU4('var x=u4(1,2,3,4000000000);', [1,2,3,4000000000]);
CheckLanesU4('var x=u4(' + INT32_MIN + ',2,3,' + UINT32_MAX + ')', [INT32_MIN>>>0,2,3,UINT32_MAX]);
CheckLanesU4('var x=u4(1,2,3,4); var y=u4(5,6,7,8)', [1,2,3,4]);
CheckLanesU4('var a=1; var b=u4(9,8,7,6); var c=13.37; var x=u4(1,2,3,4); var y=u4(5,6,7,8)', [1,2,3,4]);
CheckLanesU4('var y=u4(5,6,7,8); var x=u4(1,2,3,4)', [1,2,3,4]);

CheckLanesF4('var x=f4(' + INT32_MAX + ', 2, 3, ' + INT32_MIN + ')', [INT32_MAX, 2, 3, INT32_MIN]);
CheckLanesF4('var x=f4(' + (INT32_MAX + 1) + ', 2, 3, 4)', [INT32_MAX + 1, 2, 3, 4]);
CheckLanesF4('var x=f4(1.3, 2.4, 3.5, 98.76)', [1.3, 2.4, 3.5, 98.76]);
CheckLanesF4('var x=f4(13.37, 2., 3., -0)', [13.37, 2, 3, -0]);

CheckLanesB4('var x=b4(0,0,0,0);', [0,0,0,0]);
CheckLanesB4('var x=b4(0,1,0,0);', [0,1,0,0]);
CheckLanesB4('var x=b4(0,2,0,0);', [0,1,0,0]);
CheckLanesB4('var x=b4(-1,0,1,-1);', [1,0,1,1]);

// 1.3.3. Variable assignments
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2, 3);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1.0, 2, 3, 4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2.0, 3, 4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2, 3.0, 4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2, 3, 4.0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4(1, 2, 3, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); var c=4.0; x=i4(1, 2, 3, +c);} return f");

assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + I32 + "var i32=new glob.Int32Array(heap); function f() {var x=i4(1,2,3,4); i32[0] = x;} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + I32 + "var i32=new glob.Int32Array(heap); function f() {var x=i4(1,2,3,4); x = i32[0];} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + F32 + "var f32=new glob.Float32Array(heap); function f() {var x=f4(1,2,3,4); f32[0] = x;} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', USE_ASM + F32 + "var f32=new glob.Int32Array(heap); function f() {var x=f4(1,2,3,4); x = f32[0];} return f");

CheckI4('', 'var x=i4(1,2,3,4); x=i4(5,6,7,8)', [5, 6, 7, 8]);
CheckI4('', 'var x=i4(1,2,3,4); var c=6; x=i4(5,c|0,7,8)', [5, 6, 7, 8]);
CheckI4('', 'var x=i4(8,7,6,5); x=i4(e(x,3)|0,e(x,2)|0,e(x,1)|0,e(x,0)|0)', [5, 6, 7, 8]);

CheckU4('', 'var x=u4(1,2,3,4); x=u4(5,6,7,4000000000)', [5, 6, 7, 4000000000]);
CheckU4('', 'var x=u4(1,2,3,4); var c=6; x=u4(5,c|0,7,8)', [5, 6, 7, 8]);
CheckU4('', 'var x=u4(8,7,6,5); x=u4(e(x,3)|0,e(x,2)|0,e(x,1)|0,e(x,0)|0)', [5, 6, 7, 8]);

assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; x=f4(1,2,3,c);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; x=f4(1.,2.,3.,c);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4.; x=f4(1,2,3,c);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4.; x=f4(1.,2.,3.,c);} return f");

CheckF4(FROUND, 'var x=f4(1,2,3,4); var y=f32(7.); x=f4(f32(5),f32(6),y,f32(8))', [5, 6, 7, 8]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); x=f4(f32(5),f32(6),f32(7),f32(8))', [5, 6, 7, 8]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); x=f4(f32(5.),f32(6.),f32(7.),f32(8.))', [5, 6, 7, 8]);
CheckF4('', 'var x=f4(1.,2.,3.,4.); x=f4(5.,6.,7.,8.)', [5, 6, 7, 8]);
CheckF4('', 'var x=f4(1.,2.,3.,4.); x=f4(1,2,3,4)', [1, 2, 3, 4]);
CheckF4(FROUND, 'var x=f4(1.,2.,3.,4.); var y=f32(7.); x=f4(9, 4, 2, 1)', [9, 4, 2, 1]);
CheckF4('', 'var x=f4(8.,7.,6.,5.); x=f4(e(x,3),e(x,2),e(x,1),e(x,0))', [5, 6, 7, 8]);

// Optimization for all lanes from the same definition.
CheckI4('', 'var x=i4(1,2,3,4); var c=6; x=i4(c|0,c|0,c|0,c|0)', [6, 6, 6, 6]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); var y=f32(7.); x=f4(y,y,y,y)', [7, 7, 7, 7]);
CheckI4('', 'var x=i4(1,2,3,4); var c=0; c=e(x,3)|0; x=i4(c,c,c,c)', [4, 4, 4, 4]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); var y=f32(0); y=e(x,2); x=f4(y,y,y,y)', [3, 3, 3, 3]);
CheckI4('', 'var x=i4(1,2,3,4); var c=0; var d=0; c=e(x,3)|0; d=e(x,3)|0; x=i4(c,d,d,c)', [4, 4, 4, 4]);
CheckF4(FROUND, 'var x=f4(1,2,3,4); var y=f32(0); var z=f32(0); y=e(x,2); z=e(x,2); x=f4(y,z,y,z)', [3, 3, 3, 3]);

// Uses in ternary conditionals
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; c=x?c:c;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; x=1?x:c;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "function f() {var x=f4(1,2,3,4); var c=4; x=1?c:x;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + I32 + "function f() {var x=f4(1,2,3,4); var y=i4(1,2,3,4); x=1?x:y;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + I32 + "function f() {var x=f4(1,2,3,4); var y=i4(1,2,3,4); x=1?y:y;} return f");
assertAsmTypeFail('glob', USE_ASM + B32 + I32 + "function f() {var x=b4(1,2,3,4); var y=i4(1,2,3,4); x=1?y:y;} return f");
assertAsmTypeFail('glob', USE_ASM + U32 + I32 + "function f() {var x=u4(1,2,3,4); var y=i4(1,2,3,4); x=1?y:y;} return f");
assertAsmTypeFail('glob', USE_ASM + U32 + I32 + "function f() {var x=i4(1,2,3,4); var y=u4(1,2,3,4); x=1?y:y;} return f");

CheckF4('', 'var x=f4(1,2,3,4); var y=f4(4,3,2,1); x=3?y:x', [4, 3, 2, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(x) {x=x|0; var v=f4(1,2,3,4); var w=f4(5,6,7,8); return cf4(x?w:v);} return f"), this)(1), [5,6,7,8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(v) {v=cf4(v); var w=f4(5,6,7,8); return cf4(4?w:v);} return f"), this)(SIMD.Float32x4(1,2,3,4)), [5,6,7,8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(v, x) {v=cf4(v); x=x|0; var w=f4(5,6,7,8); return cf4(x?w:v);} return f"), this)(SIMD.Float32x4(1,2,3,4), 0), [1,2,3,4]);

CheckI4('', 'var x=i4(1,2,3,4); var y=i4(4,3,2,1); x=e(x,0)?y:x', [4, 3, 2, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(x) {x=x|0; var v=i4(1,2,3,4); var w=i4(5,6,7,8); return ci4(x?w:v);} return f"), this)(1), [5,6,7,8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(v) {v=ci4(v); var w=i4(5,6,7,8); return ci4(4?w:v);} return f"), this)(SIMD.Int32x4(1,2,3,4)), [5,6,7,8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(v, x) {v=ci4(v); x=x|0; var w=i4(5,6,7,8); return ci4(x?w:v);} return f"), this)(SIMD.Int32x4(1,2,3,4), 0), [1,2,3,4]);

// Unsigned SIMD types can't be function arguments or return values.
assertAsmTypeFail('glob', USE_ASM + U32 + CU32 + "function f(x) {x=cu4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + U32 + CU32 + "function f() {x=u4(0,0,0,0); return cu4(x);} return f");

// 1.3.4 Return values
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {var x=1; return ci4(x)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {var x=1; return ci4(x + x)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {var x=1.; return ci4(x)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + FROUND + "function f() {var x=f32(1.); return ci4(x)} return f");

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f() {var x=i4(1,2,3,4); return ci4(x)} return f"), this)(), [1,2,3,4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f() {var x=f4(1,2,3,4); return cf4(x)} return f"), this)(), [1,2,3,4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + B32 + CB32 + "function f() {var x=b4(1,2,0,4); return cb4(x)} return f"), this)(), [true,true,false,true]);

// 1.3.5 Coerce and pass arguments
// Via check
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {ci4();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); ci4(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {ci4(1);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f() {ci4(1.);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + FROUND + "function f() {ci4(f32(1.));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + F32 + CF32 + "function f(x) {x=cf4(x); ci4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); return 1 + ci4(x) | 0;} return f");

var i32x4 = SIMD.Int32x4(1, 3, 3, 7);
assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x)} return f"), this)(i32x4), undefined);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); return ci4(x);} return f"), this)(i32x4), [1,3,3,7]);

assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f() {cf4();} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); cf4(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f() {cf4(1);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f() {cf4(1.);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + FROUND + "function f() {cf4(f32(1.));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + F32 + CF32 + "function f(x) {x=cf4(x); cf4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); return 1 + cf4(x) | 0;} return f");

var f32x4 = SIMD.Float32x4(13.37, 42.42, -0, NaN);
assertEq(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x)} return f"), this)(f32x4), undefined);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); return cf4(x);} return f"), this)(f32x4), [13.37, 42.42, -0, NaN].map(Math.fround));

var b32x4 = SIMD.Bool32x4(true, false, false, true);
assertEq(asmLink(asmCompile('glob', USE_ASM + B32 + CB32 + "function f(x) {x=cb4(x)} return f"), this)(b32x4), undefined);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + B32 + CB32 + "function f(x) {x=cb4(x); return cb4(x);} return f"), this)(b32x4), [true, false, false, true]);

// Legacy coercions
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {x=i4();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {x=i4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {x=i4(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x,y) {x=i4(y);y=+y} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); return i4(x);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {return +i4(1,2,3,4)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "function f(x) {return 0|i4(1,2,3,4)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + FROUND + "function f(x) {return f32(i4(1,2,3,4))} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "function f(x) {return f4(i4(1,2,3,4))} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); return f4(x);} return f");


function assertCaught(f) {
    var caught = false;
    try {
        f.apply(null, Array.prototype.slice.call(arguments, 1));
    } catch (e) {
        DEBUG && print('Assert caught: ', e, '\n', e.stack);
        assertEq(e instanceof TypeError, true);
        caught = true;
    }
    assertEq(caught, true);
}

var f = asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + "function f(x) {x=cf4(x); return cf4(x);} return f"), this);
assertCaught(f);
assertCaught(f, 1);
assertCaught(f, {});
assertCaught(f, "I sincerely am a SIMD typed object.");
assertCaught(f, SIMD.Int32x4(1,2,3,4));
assertCaught(f, SIMD.Bool32x4(true, true, false, true));

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + "function f(x) {x=ci4(x); return ci4(x);} return f"), this);
assertCaught(f);
assertCaught(f, 1);
assertCaught(f, {});
assertCaught(f, "I sincerely am a SIMD typed object.");
assertCaught(f, SIMD.Float32x4(4,3,2,1));
assertCaught(f, SIMD.Bool32x4(true, true, false, true));

var f = asmLink(asmCompile('glob', USE_ASM + B32 + CB32 + "function f(x) {x=cb4(x); return cb4(x);} return f"), this);
assertCaught(f);
assertCaught(f, 1);
assertCaught(f, {});
assertCaught(f, "I sincerely am a SIMD typed object.");
assertCaught(f, SIMD.Int32x4(1,2,3,4));
assertCaught(f, SIMD.Float32x4(4,3,2,1));

// 1.3.6 Globals
// 1.3.6.1 Local globals
// Read
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); function f() {var x=4; x=g|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); function f() {var x=4.; x=+g;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); var f32=glob.Math.fround; function f() {var x=f32(4.); x=f32(g);} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); function f() {var x=4; x=g|0;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); function f() {var x=4.; x=+g;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); var f32=glob.Math.fround; function f() {var x=f32(4.); x=f32(g);} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + I32 + CI32 + "var g=f4(1., 2., 3., 4.); function f() {var x=i4(1,2,3,4); x=ci4(g);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + "var g=i4(1,2,3,4); function f() {var x=f4(1.,2.,3.,4.); x=cf4(g);} return f");
assertAsmTypeFail('glob', USE_ASM + U32 + I32 + CI32 + "var g=u4(1,2,3,4); function f() {var x=i4(1,2,3,4); x=ci4(g);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + "var g=0; function f() {var x=i4(1,2,3,4); x=g|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=0.; function f() {var x=i4(1,2,3,4); x=+g;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var f32=glob.Math.fround; var g=f32(0.); function f() {var x=i4(1,2,3,4); x=f32(g);} return f");

// Unsigned SIMD globals are not allowed.
assertAsmTypeFail('glob', USE_ASM + U32 + "var g=u4(0,0,0,0); function f() {var x=u4(1,2,3,4); x=g;} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + "var g=0; function f() {var x=f4(0.,0.,0.,0.); x=g|0;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=0.; function f() {var x=f4(0.,0.,0.,0.); x=+g;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var f32=glob.Math.fround; var g=f32(0.); function f() {var x=f4(0.,0.,0.,0.); x=f32(g);} return f");

CheckI4('var x=i4(1,2,3,4)', '', [1, 2, 3, 4]);
CheckI4('var _=42; var h=i4(5,5,5,5); var __=13.37; var x=i4(4,7,9,2);', '', [4,7,9,2]);

CheckF4('var x=f4(1.,2.,3.,4.)', '', [1, 2, 3, 4]);
CheckF4('var _=42; var h=f4(5.,5.,5.,5.); var __=13.37; var x=f4(4.,13.37,9.,-0.);', '', [4, 13.37, 9, -0]);
CheckF4('var x=f4(1,2,3,4)', '', [1, 2, 3, 4]);

CheckB4('var x=b4(1,0,3,0)', '', [true, false, true, false]);
CheckB4('var _=42; var h=b4(5,0,5,5); var __=13.37; var x=b4(0,0,9,2);', '', [false, false, true, true]);

// Write
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); function f() {var x=4; g=x|0;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); function f() {var x=4.; g=+x;} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var g=i4(1,2,3,4); var f32=glob.Math.fround; function f() {var x=f32(4.); g=f32(x);} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); function f() {var x=4; g=x|0;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); function f() {var x=4.; g=+x;} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + "var g=f4(1., 2., 3., 4.); var f32=glob.Math.fround; function f() {var x=f32(4.); g=f32(x);} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + I32 + CI32 + "var g=f4(1., 2., 3., 4.); function f() {var x=i4(1,2,3,4); g=ci4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + I32 + CF32 + "var g=f4(1., 2., 3., 4.); function f() {var x=i4(1,2,3,4); g=cf4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + "var g=i4(1,2,3,4); function f() {var x=f4(1.,2.,3.,4.); g=cf4(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CI32 + "var g=i4(1,2,3,4); function f() {var x=f4(1.,2.,3.,4.); g=ci4(x);} return f");

CheckI4('var x=i4(0,0,0,0);', 'x=i4(1,2,3,4)', [1,2,3,4]);
CheckF4('var x=f4(0.,0.,0.,0.);', 'x=f4(5.,3.,4.,2.)', [5,3,4,2]);
CheckB4('var x=b4(0,0,0,0);', 'x=b4(0,0,1,1)', [false, false, true, true]);

CheckI4('var x=i4(0,0,0,0); var y=42; var z=3.9; var w=13.37', 'x=i4(1,2,3,4); y=24; z=4.9; w=23.10;', [1,2,3,4]);
CheckF4('var x=f4(0,0,0,0); var y=42; var z=3.9; var w=13.37', 'x=f4(1,2,3,4); y=24; z=4.9; w=23.10;', [1,2,3,4]);
CheckB4('var x=b4(0,0,0,0); var y=42; var z=3.9; var w=13.37', 'x=b4(1,0,0,0); y=24; z=4.9; w=23.10;', [true, false, false, false]);

// 1.3.6.2 Imported globals
// Read
var Int32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + I32 + CI32 + "var g=ci4(ffi.g); function f() {return ci4(g)} return f"), this, {g: SIMD.Int32x4(1,2,3,4)})();
assertEq(SIMD.Int32x4.extractLane(Int32x4, 0), 1);
assertEq(SIMD.Int32x4.extractLane(Int32x4, 1), 2);
assertEq(SIMD.Int32x4.extractLane(Int32x4, 2), 3);
assertEq(SIMD.Int32x4.extractLane(Int32x4, 3), 4);

for (var v of [1, {}, "totally legit SIMD variable", SIMD.Float32x4(1,2,3,4)])
    assertCaught(asmCompile('glob', 'ffi', USE_ASM + I32 + CI32 + "var g=ci4(ffi.g); function f() {return ci4(g)} return f"), this, {g: v});

// Unsigned SIMD globals are not allowed.
assertAsmTypeFail('glob', 'ffi', USE_ASM + U32 + CU32 + "var g=cu4(ffi.g); function f() {} return f");

var Float32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + F32 + CF32 + "var g=cf4(ffi.g); function f() {return cf4(g)} return f"), this, {g: SIMD.Float32x4(1,2,3,4)})();
assertEq(SIMD.Float32x4.extractLane(Float32x4, 0), 1);
assertEq(SIMD.Float32x4.extractLane(Float32x4, 1), 2);
assertEq(SIMD.Float32x4.extractLane(Float32x4, 2), 3);
assertEq(SIMD.Float32x4.extractLane(Float32x4, 3), 4);

for (var v of [1, {}, "totally legit SIMD variable", SIMD.Int32x4(1,2,3,4)])
    assertCaught(asmCompile('glob', 'ffi', USE_ASM + F32 + CF32 + "var g=cf4(ffi.g); function f() {return cf4(g)} return f"), this, {g: v});

var Bool32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + B32 + CB32 + "var g=cb4(ffi.g); function f() {return cb4(g)} return f"), this, {g: SIMD.Bool32x4(false, false, false, true)})();
assertEq(SIMD.Bool32x4.extractLane(Bool32x4, 0), false);
assertEq(SIMD.Bool32x4.extractLane(Bool32x4, 1), false);
assertEq(SIMD.Bool32x4.extractLane(Bool32x4, 2), false);
assertEq(SIMD.Bool32x4.extractLane(Bool32x4, 3), true);

for (var v of [1, {}, "totally legit SIMD variable", SIMD.Int32x4(1,2,3,4)])
    assertCaught(asmCompile('glob', 'ffi', USE_ASM + B32 + CB32 + "var g=cb4(ffi.g); function f() {return cb4(g)} return f"), this, {g: v});

// Write
var Int32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + I32 + CI32 + "var g=ci4(ffi.g); function f() {g=i4(4,5,6,7); return ci4(g)} return f"), this, {g: SIMD.Int32x4(1,2,3,4)})();
assertEq(SIMD.Int32x4.extractLane(Int32x4, 0), 4);
assertEq(SIMD.Int32x4.extractLane(Int32x4, 1), 5);
assertEq(SIMD.Int32x4.extractLane(Int32x4, 2), 6);
assertEq(SIMD.Int32x4.extractLane(Int32x4, 3), 7);

var Float32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + F32 + CF32 + "var g=cf4(ffi.g); function f() {g=f4(4.,5.,6.,7.); return cf4(g)} return f"), this, {g: SIMD.Float32x4(1,2,3,4)})();
assertEq(SIMD.Float32x4.extractLane(Float32x4, 0), 4);
assertEq(SIMD.Float32x4.extractLane(Float32x4, 1), 5);
assertEq(SIMD.Float32x4.extractLane(Float32x4, 2), 6);
assertEq(SIMD.Float32x4.extractLane(Float32x4, 3), 7);

var Bool32x4 = asmLink(asmCompile('glob', 'ffi', USE_ASM + B32 + CB32 + "var g=cb4(ffi.g); function f() {g=b4(1,1,0,0); return cb4(g)} return f"), this, {g: SIMD.Bool32x4(1,1,1,0)})();
assertEq(SIMD.Bool32x4.extractLane(Bool32x4, 0), true);
assertEq(SIMD.Bool32x4.extractLane(Bool32x4, 1), true);
assertEq(SIMD.Bool32x4.extractLane(Bool32x4, 2), false);
assertEq(SIMD.Bool32x4.extractLane(Bool32x4, 3), false);

// 2. SIMD operations
// 2.1 Compilation
assertAsmTypeFail('glob', USE_ASM + "var add = Int32x4.add; return {}");
assertAsmTypeFail('glob', USE_ASM + I32A + I32 + "return {}");
assertAsmTypeFail('glob', USE_ASM + "var g = 3; var add = g.add; return {}");
assertAsmTypeFail('glob', USE_ASM + I32 + "var func = i4.doTheHarlemShake; return {}");
assertAsmTypeFail('glob', USE_ASM + I32 + "var div = i4.div; return {}");
assertAsmTypeFail('glob', USE_ASM + "var f32 = glob.Math.fround; var i4a = f32.add; return {}");
// Operation exists, but in a different type.
assertAsmTypeFail('glob', USE_ASM + I32 + "var func = i4.fromUint32x4; return {}");

// 2.2 Linking
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + I32 + I32A + "function f() {} return f"), {});
assertAsmLinkAlwaysFail(asmCompile('glob', USE_ASM + I32 + I32A + "function f() {} return f"), {SIMD: Math.fround});

var oldInt32x4Add = SIMD.Int32x4.add;
var code = asmCompile('glob', USE_ASM + I32 + I32A + "return {}");
for (var v of [42, Math.fround, SIMD.Float32x4.add, function(){}, SIMD.Int32x4.mul]) {
    SIMD.Int32x4.add = v;
    assertAsmLinkFail(code, {SIMD: {Int32x4: SIMD.Int32x4}});
}
SIMD.Int32x4.add = oldInt32x4Add; // finally replace the add function with the original one
assertEq(asmLink(asmCompile('glob', USE_ASM + I32 + I32A + "function f() {} return f"), {SIMD: {Int32x4: SIMD.Int32x4}})(), undefined);

// 2.3. Binary arithmetic operations
// 2.3.1 Additions
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a();} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(x, x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(13, 37);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(23.10, 19.89);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(x, 42);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); x=i4a(x, 13.37);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(1,2,3,4); var y=4; x=i4a(x, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(0,0,0,0); var y=4; x=i4a(y, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + "function f() {var x=i4(0,0,0,0); var y=4; y=i4a(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + U32 + "function f() {var x=i4(0,0,0,0); var y=u4(1,2,3,4); y=i4a(x, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + I32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); x=i4a(x, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + I32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); y=i4a(x, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + I32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); y=i4a(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + F32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); y=f4a(x, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + F32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); y=f4a(x, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + F32A + "function f() {var x=i4(0,0,0,0); var y=f4(4,3,2,1); x=f4a(y, y);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + I32A + 'function f() {var x=i4(1,2,3,4); var y=0; y=i4a(x,x)|0} return f');
assertAsmTypeFail('glob', USE_ASM + I32 + I32A + 'function f() {var x=i4(1,2,3,4); var y=0.; y=+i4a(x,x)} return f');

CheckI4(I32A, 'var z=i4(1,2,3,4); var y=i4(0,1,0,3); var x=i4(0,0,0,0); x=i4a(z,y)', [1,3,3,7]);
CheckI4(I32A, 'var x=i4(2,3,4,5); var y=i4(0,1,0,3); x=i4a(x,y)', [2,4,4,8]);
CheckI4(I32A, 'var x=i4(1,2,3,4); x=i4a(x,x)', [2,4,6,8]);
CheckI4(I32A, 'var x=i4(' + INT32_MAX + ',2,3,4); var y=i4(1,1,0,3); x=i4a(x,y)', [INT32_MIN,3,3,7]);
CheckI4(I32A, 'var x=i4(' + INT32_MAX + ',2,3,4); var y=i4(1,1,0,3); x=ci4(i4a(x,y))', [INT32_MIN,3,3,7]);

CheckU4(U32A, 'var z=u4(1,2,3,4); var y=u4(0,1,0,3); var x=u4(0,0,0,0); x=u4a(z,y)', [1,3,3,7]);
CheckU4(U32A, 'var x=u4(2,3,4,5); var y=u4(0,1,0,3); x=u4a(x,y)', [2,4,4,8]);
CheckU4(U32A, 'var x=u4(1,2,3,4); x=u4a(x,x)', [2,4,6,8]);

CheckF4(F32A, 'var x=f4(1,2,3,4); x=f4a(x,x)', [2,4,6,8]);
CheckF4(F32A, 'var x=f4(1,2,3,4); var y=f4(4,3,5,2); x=f4a(x,y)', [5,5,8,6]);
CheckF4(F32A, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=f4a(x,y)', [Math.fround(13.37) + 4,5,8,6]);
CheckF4(F32A, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=cf4(f4a(x,y))', [Math.fround(13.37) + 4,5,8,6]);

// 2.3.2. Subtracts
CheckI4(I32S, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=i4s(x,y)', [2,1,3,2]);
CheckI4(I32S, 'var x=i4(5,4,3,2); var y=i4(1,2,3,4); x=i4s(x,y)', [4,2,0,-2]);
CheckI4(I32S, 'var x=i4(1,2,3,4); x=i4s(x,x)', [0,0,0,0]);
CheckI4(I32S, 'var x=i4(' + INT32_MIN + ',2,3,4); var y=i4(1,1,0,3); x=i4s(x,y)', [INT32_MAX,1,3,1]);
CheckI4(I32S, 'var x=i4(' + INT32_MIN + ',2,3,4); var y=i4(1,1,0,3); x=ci4(i4s(x,y))', [INT32_MAX,1,3,1]);

CheckU4(U32S, 'var x=u4(1,2,3,4); var y=u4(-1,1,0,2); x=u4s(x,y)', [2,1,3,2]);
CheckU4(U32S, 'var x=u4(5,4,3,2); var y=u4(1,2,3,4); x=u4s(x,y)', [4,2,0,-2>>>0]);
CheckU4(U32S, 'var x=u4(1,2,3,4); x=u4s(x,x)', [0,0,0,0]);
CheckU4(U32S, 'var x=u4(' + INT32_MIN + ',2,3,4); var y=u4(1,1,0,3); x=u4s(x,y)', [INT32_MAX,1,3,1]);
CheckU4(U32S, 'var x=u4(' + INT32_MIN + ',2,3,4); var y=u4(1,1,0,3); x=cu4(u4s(x,y))', [INT32_MAX,1,3,1]);

CheckF4(F32S, 'var x=f4(1,2,3,4); x=f4s(x,x)', [0,0,0,0]);
CheckF4(F32S, 'var x=f4(1,2,3,4); var y=f4(4,3,5,2); x=f4s(x,y)', [-3,-1,-2,2]);
CheckF4(F32S, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=f4s(x,y)', [Math.fround(13.37) - 4,-1,-2,2]);
CheckF4(F32S, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=cf4(f4s(x,y))', [Math.fround(13.37) - 4,-1,-2,2]);

{
    // Bug 1216099
    let code = `
        "use asm";
        var f4 = global.SIMD.Float32x4;
        var f4sub = f4.sub;
        const zerox4 = f4(0.0, 0.0, 0.0, 0.0);
        function f() {
            var newVelx4 = f4(0.0, 0.0, 0.0, 0.0);
            var newVelTruex4 = f4(0.0,0.0,0.0,0.0);
            newVelTruex4 = f4sub(zerox4, newVelx4);
        }
        // return statement voluntarily missing
    `;
    assertAsmTypeFail('global', code);
}

// 2.3.3. Multiplications / Divisions
assertAsmTypeFail('glob', USE_ASM + I32 + "var f4d=i4.div; function f() {} return f");

CheckI4(I32M, 'var x=i4(1,2,3,4); var y=i4(-1,1,0,2); x=i4m(x,y)', [-1,2,0,8]);
CheckI4(I32M, 'var x=i4(5,4,3,2); var y=i4(1,2,3,4); x=i4m(x,y)', [5,8,9,8]);
CheckI4(I32M, 'var x=i4(1,2,3,4); x=i4m(x,x)', [1,4,9,16]);
(function() {
    var m = INT32_MIN, M = INT32_MAX, imul = Math.imul;
    CheckI4(I32M, `var x=i4(${m},${m}, ${M}, ${M}); var y=i4(2,-3,4,-5); x=i4m(x,y)`,
            [imul(m, 2), imul(m, -3), imul(M, 4), imul(M, -5)]);
    CheckI4(I32M, `var x=i4(${m},${m}, ${M}, ${M}); var y=i4(${m}, ${M}, ${m}, ${M}); x=i4m(x,y)`,
            [imul(m, m), imul(m, M), imul(M, m), imul(M, M)]);
})();

CheckF4(F32M, 'var x=f4(1,2,3,4); x=f4m(x,x)', [1,4,9,16]);
CheckF4(F32M, 'var x=f4(1,2,3,4); var y=f4(4,3,5,2); x=f4m(x,y)', [4,6,15,8]);
CheckF4(F32M, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=f4m(x,y)', [Math.fround(13.37) * 4,6,15,8]);
CheckF4(F32M, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=cf4(f4m(x,y))', [Math.fround(13.37) * 4,6,15,8]);

var f32x4 = SIMD.Float32x4(0, NaN, -0, NaN);
var another = SIMD.Float32x4(NaN, -1, -0, NaN);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + F32M + CF32 + "function f(x, y) {x=cf4(x); y=cf4(y); x=f4m(x,y); return cf4(x);} return f"), this)(f32x4, another), [NaN, NaN, 0, NaN]);

CheckF4(F32D, 'var x=f4(1,2,3,4); x=f4d(x,x)', [1,1,1,1]);
CheckF4(F32D, 'var x=f4(1,2,3,4); var y=f4(4,3,5,2); x=f4d(x,y)', [1/4,2/3,3/5,2]);
CheckF4(F32D, 'var x=f4(13.37,1,1,4); var y=f4(4,0,-0.,2); x=f4d(x,y)', [Math.fround(13.37) / 4,+Infinity,-Infinity,2]);

var f32x4 = SIMD.Float32x4(0, 0, -0, NaN);
var another = SIMD.Float32x4(0, -0, 0, 0);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + F32D + CF32 + "function f(x,y) {x=cf4(x); y=cf4(y); x=f4d(x,y); return cf4(x);} return f"), this)(f32x4, another), [NaN, NaN, NaN, NaN]);

// Unary arithmetic operators
function CheckUnaryF4(op, checkFunc, assertFunc) {
    var _ = asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + 'var op=f4.' + op + '; function f(x){x=cf4(x); return cf4(op(x)); } return f'), this);
    return function(input) {
        var simd = SIMD.Float32x4(input[0], input[1], input[2], input[3]);

        var exp = input.map(Math.fround).map(checkFunc).map(Math.fround);
        var obs = _(simd);
        assertEqX4(obs, exp, assertFunc);
    }
}

function CheckUnaryI4(op, checkFunc) {
    var _ = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + 'var op=i4.' + op + '; function f(x){x=ci4(x); return ci4(op(x)); } return f'), this);
    return function(input) {
        var simd = SIMD.Int32x4(input[0], input[1], input[2], input[3]);
        assertEqX4(_(simd), input.map(checkFunc).map(function(x) { return x | 0}));
    }
}

function CheckUnaryU4(op, checkFunc) {
    var _ = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32U32 + U32 + U32I32 +
                               'var op=u4.' + op + '; function f(x){x=ci4(x); return ci4(i4u4(op(u4i4(x)))); } return f'), this);
    return function(input) {
        var simd = SIMD.Int32x4(input[0], input[1], input[2], input[3]);
        var res = SIMD.Uint32x4.fromInt32x4Bits(_(simd));
        assertEqX4(res, input.map(checkFunc).map(function(x) { return x >>> 0 }));
    }
}

function CheckUnaryB4(op, checkFunc) {
    var _ = asmLink(asmCompile('glob', USE_ASM + B32 + CB32 + 'var op=b4.' + op + '; function f(x){x=cb4(x); return cb4(op(x)); } return f'), this);
    return function(input) {
        var simd = SIMD.Bool32x4(input[0], input[1], input[2], input[3]);
        assertEqX4(_(simd), input.map(checkFunc).map(function(x) { return !!x}));
    }
}

CheckUnaryI4('neg', function(x) { return -x })([1, -2, INT32_MIN, INT32_MAX]);
CheckUnaryI4('not', function(x) { return ~x })([1, -2, INT32_MIN, INT32_MAX]);

CheckUnaryU4('neg', function(x) { return -x })([1, -2, INT32_MIN, INT32_MAX]);
CheckUnaryU4('not', function(x) { return ~x })([1, -2, INT32_MIN, INT32_MAX]);

var CheckNotB = CheckUnaryB4('not', function(x) { return !x });
CheckNotB([true, false, true, true]);
CheckNotB([true, true, true, true]);
CheckNotB([false, false, false, false]);

var CheckAbs = CheckUnaryF4('abs', Math.abs);
CheckAbs([1, 42.42, 0.63, 13.37]);
CheckAbs([NaN, -Infinity, Infinity, 0]);

var CheckNegF = CheckUnaryF4('neg', function(x) { return -x });
CheckNegF([1, 42.42, 0.63, 13.37]);
CheckNegF([NaN, -Infinity, Infinity, 0]);

var CheckSqrt = CheckUnaryF4('sqrt', function(x) { return Math.sqrt(x); });
CheckSqrt([1, 42.42, 0.63, 13.37]);
CheckSqrt([NaN, -Infinity, Infinity, 0]);

// ReciprocalApproximation and reciprocalSqrtApproximation give approximate results
function assertNear(a, b) {
    if (a !== a && b === b)
        throw 'Observed NaN, expected ' + b;
    if (Math.abs(a - b) > 1e-3)
        throw 'More than 1e-3 between ' + a + ' and ' + b;
}
var CheckRecp = CheckUnaryF4('reciprocalApproximation', function(x) { return 1 / x; }, assertNear);
CheckRecp([1, 42.42, 0.63, 13.37]);
CheckRecp([NaN, -Infinity, Infinity, 0]);

var CheckRecp = CheckUnaryF4('reciprocalSqrtApproximation', function(x) { return 1 / Math.sqrt(x); }, assertNear);
CheckRecp([1, 42.42, 0.63, 13.37]);
CheckRecp([NaN, -Infinity, Infinity, 0]);

// Min/Max
assertAsmTypeFail('glob', USE_ASM + I32 + "var f4m=i4.min; function f() {} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var f4d=i4.max; function f() {} return f");

const F32MIN = 'var min = f4.min;'
const F32MAX = 'var max = f4.max;'

CheckF4(F32MIN, 'var x=f4(1,2,3,4); x=min(x,x)', [1,2,3,4]);
CheckF4(F32MIN, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=min(x,y)', [4,2,3,2]);
CheckF4(F32MIN + FROUND + 'var Infinity = glob.Infinity;', 'var x=f4(0,0,0,0); var y=f4(2310,3,5,0); x=f4(f32(+Infinity),f32(-Infinity),f32(3),f32(-0.)); x=min(x,y)', [2310,-Infinity,3,-0]);

CheckF4(F32MIN, 'var x=f4(0,0,-0,-0); var y=f4(0,-0,0,-0); x=min(x,y)', [0,-0,-0,-0]);
CheckF4(F32MIN + FROUND + 'var NaN = glob.NaN;', 'var x=f4(0,0,0,0); var y=f4(0,0,0,0); var n=f32(0); n=f32(NaN); x=f4(n,0.,n,0.); y=f4(n,n,0.,0.); x=min(x,y)', [NaN, NaN, NaN, 0]);

CheckF4(F32MAX, 'var x=f4(1,2,3,4); x=max(x,x)', [1,2,3,4]);
CheckF4(F32MAX, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=max(x,y)', [13.37, 3, 5, 4]);
CheckF4(F32MAX + FROUND + 'var Infinity = glob.Infinity;', 'var x=f4(0,0,0,0); var y=f4(2310,3,5,0); x=f4(f32(+Infinity),f32(-Infinity),f32(3),f32(-0.)); x=max(x,y)', [+Infinity,3,5,0]);

CheckF4(F32MAX, 'var x=f4(0,0,-0,-0); var y=f4(0,-0,0,-0); x=max(x,y)', [0,0,0,-0]);
CheckF4(F32MAX + FROUND + 'var NaN = glob.NaN;', 'var x=f4(0,0,0,0); var y=f4(0,0,0,0); var n=f32(0); n=f32(NaN); x=f4(n,0.,n,0.); y=f4(n,n,0.,0.); x=max(x,y)', [NaN, NaN, NaN, 0]);

const F32MINNUM = 'var min = f4.minNum;'
const F32MAXNUM = 'var max = f4.maxNum;'

CheckF4(F32MINNUM, 'var x=f4(1,2,3,4); x=min(x,x)', [1,2,3,4]);
CheckF4(F32MINNUM, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=min(x,y)', [4,2,3,2]);
CheckF4(F32MINNUM + FROUND + 'var Infinity = glob.Infinity;', 'var x=f4(0,0,0,0); var y=f4(2310,3,5,0); x=f4(f32(+Infinity),f32(-Infinity),f32(3),f32(-0.)); x=min(x,y)', [2310,-Infinity,3,-0]);

CheckF4(F32MINNUM, 'var x=f4(0,0,-0,-0); var y=f4(0,-0,0,-0); x=min(x,y)', [0,-0,-0,-0]);
CheckF4(F32MINNUM + FROUND + 'var NaN = glob.NaN;', 'var x=f4(0,0,0,0); var y=f4(0,0,0,0); var n=f32(0); n=f32(NaN); x=f4(n,0.,n,0.); y=f4(n,n,0.,0.); x=min(x,y)', [NaN, 0, 0, 0]);

CheckF4(F32MAXNUM, 'var x=f4(1,2,3,4); x=max(x,x)', [1,2,3,4]);
CheckF4(F32MAXNUM, 'var x=f4(13.37,2,3,4); var y=f4(4,3,5,2); x=max(x,y)', [13.37, 3, 5, 4]);
CheckF4(F32MAXNUM + FROUND + 'var Infinity = glob.Infinity;', 'var x=f4(0,0,0,0); var y=f4(2310,3,5,0); x=f4(f32(+Infinity),f32(-Infinity),f32(3),f32(-0.)); x=max(x,y)', [+Infinity,3,5,0]);

CheckF4(F32MAXNUM, 'var x=f4(0,0,-0,-0); var y=f4(0,-0,0,-0); x=max(x,y)', [0,0,0,-0]);
CheckF4(F32MAXNUM + FROUND + 'var NaN = glob.NaN;', 'var x=f4(0,0,0,0); var y=f4(0,0,0,0); var n=f32(0); n=f32(NaN); x=f4(n,0.,n,0.); y=f4(n,n,0.,0.); x=max(x,y)', [NaN, 0, 0, 0]);

// ReplaceLane
const RLF = 'var r = f4.replaceLane;';

assertAsmTypeFail('glob', USE_ASM + F32 + RLF + "function f() {var x = f4(1,2,3,4); x = r(x, 0, 1);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + "function f() {var x = f4(1,2,3,4); x = r(x, 0, x);} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(x, 4, f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(x, f32(0), f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(1, 0, f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(1, 0., f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); x = r(f32(1), 0, f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); var l = 0; x = r(x, l, f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + RLF + FROUND + "function f() {var x = f4(1,2,3,4); var y = i4(1,2,3,4); x = r(y, 0, f32(1));} return f");

CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 0, f32(13.37));', [Math.fround(13.37), 2, 3, 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 1, f32(13.37));', [1, Math.fround(13.37), 3, 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 2, f32(13.37));', [1, 2, Math.fround(13.37), 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 3, f32(13.37));', [1, 2, 3, Math.fround(13.37)]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 3, f32(13.37) + f32(6.63));', [1, 2, 3, Math.fround(Math.fround(13.37) + Math.fround(6.63))]);

CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 0, 13.37);', [Math.fround(13.37), 2, 3, 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 1, 13.37);', [1, Math.fround(13.37), 3, 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 2, 13.37);', [1, 2, Math.fround(13.37), 4]);
CheckF4(RLF + FROUND, 'var x = f4(1,2,3,4); x = r(x, 3, 13.37);', [1, 2, 3, Math.fround(13.37)]);

const RLI = 'var r = i4.replaceLane;';
CheckI4(RLI, 'var x = i4(1,2,3,4); x = r(x, 0, 42);', [42, 2, 3, 4]);
CheckI4(RLI, 'var x = i4(1,2,3,4); x = r(x, 1, 42);', [1, 42, 3, 4]);
CheckI4(RLI, 'var x = i4(1,2,3,4); x = r(x, 2, 42);', [1, 2, 42, 4]);
CheckI4(RLI, 'var x = i4(1,2,3,4); x = r(x, 3, 42);', [1, 2, 3, 42]);

const RLU = 'var r = u4.replaceLane;';
CheckU4(RLU, 'var x = u4(1,2,3,4); x = r(x, 0, 42);', [42, 2, 3, 4]);
CheckU4(RLU, 'var x = u4(1,2,3,4); x = r(x, 1, 42);', [1, 42, 3, 4]);
CheckU4(RLU, 'var x = u4(1,2,3,4); x = r(x, 2, 42);', [1, 2, 42, 4]);
CheckU4(RLU, 'var x = u4(1,2,3,4); x = r(x, 3, 42);', [1, 2, 3, 42]);

const RLB = 'var r = b4.replaceLane;';
CheckB4(RLB, 'var x = b4(1,1,0,0); x = r(x, 0, 0);', [false, true, false, false]);
CheckB4(RLB, 'var x = b4(1,1,0,0); x = r(x, 1, 0);', [true, false, false, false]);
CheckB4(RLB, 'var x = b4(1,1,0,0); x = r(x, 2, 2);', [true, true, true, false]);
CheckB4(RLB, 'var x = b4(1,1,0,0); x = r(x, 3, 1);', [true, true, false, true]);

// Comparisons
// Comparison operators produce Bool32x4 vectors.
const T = true;
const F = false;

const EQI32 = 'var eq = i4.equal';
const NEI32 = 'var ne = i4.notEqual';
const LTI32 = 'var lt = i4.lessThan;';
const LEI32 = 'var le = i4.lessThanOrEqual';
const GTI32 = 'var gt = i4.greaterThan;';
const GEI32 = 'var ge = i4.greaterThanOrEqual';

CheckB4(I32+EQI32, 'var x=b4(0,0,0,0); var a=i4(1,2,3,4);  var b=i4(-1,1,0,2); x=eq(a,b)', [F, F, F, F]);
CheckB4(I32+EQI32, 'var x=b4(0,0,0,0); var a=i4(-1,1,0,2); var b=i4(1,2,3,4);  x=eq(a,b)', [F, F, F, F]);
CheckB4(I32+EQI32, 'var x=b4(0,0,0,0); var a=i4(1,0,3,4);  var b=i4(1,1,7,0);  x=eq(a,b)', [T, F, F, F]);

CheckB4(I32+NEI32, 'var x=b4(0,0,0,0); var a=i4(1,2,3,4);  var b=i4(-1,1,0,2); x=ne(a,b)', [T, T, T, T]);
CheckB4(I32+NEI32, 'var x=b4(0,0,0,0); var a=i4(-1,1,0,2); var b=i4(1,2,3,4);  x=ne(a,b)', [T, T, T, T]);
CheckB4(I32+NEI32, 'var x=b4(0,0,0,0); var a=i4(1,0,3,4);  var b=i4(1,1,7,0);  x=ne(a,b)', [F, T, T, T]);

CheckB4(I32+LTI32, 'var x=b4(0,0,0,0); var a=i4(1,2,3,4);  var b=i4(-1,1,0,2); x=lt(a,b)', [F, F, F, F]);
CheckB4(I32+LTI32, 'var x=b4(0,0,0,0); var a=i4(-1,1,0,2); var b=i4(1,2,3,4);  x=lt(a,b)', [T, T, T, T]);
CheckB4(I32+LTI32, 'var x=b4(0,0,0,0); var a=i4(1,0,3,4);  var b=i4(1,1,7,0);  x=lt(a,b)', [F, T, T, F]);

CheckB4(I32+LEI32, 'var x=b4(0,0,0,0); var a=i4(1,2,3,4);  var b=i4(-1,1,0,2); x=le(a,b)', [F, F, F, F]);
CheckB4(I32+LEI32, 'var x=b4(0,0,0,0); var a=i4(-1,1,0,2); var b=i4(1,2,3,4);  x=le(a,b)', [T, T, T, T]);
CheckB4(I32+LEI32, 'var x=b4(0,0,0,0); var a=i4(1,0,3,4);  var b=i4(1,1,7,0);  x=le(a,b)', [T, T, T, F]);

CheckB4(I32+GTI32, 'var x=b4(0,0,0,0); var a=i4(1,2,3,4);  var b=i4(-1,1,0,2); x=gt(a,b)', [T, T, T, T]);
CheckB4(I32+GTI32, 'var x=b4(0,0,0,0); var a=i4(-1,1,0,2); var b=i4(1,2,3,4);  x=gt(a,b)', [F, F, F, F]);
CheckB4(I32+GTI32, 'var x=b4(0,0,0,0); var a=i4(1,0,3,4);  var b=i4(1,1,7,0);  x=gt(a,b)', [F, F, F, T]);

CheckB4(I32+GEI32, 'var x=b4(0,0,0,0); var a=i4(1,2,3,4);  var b=i4(-1,1,0,2); x=ge(a,b)', [T, T, T, T]);
CheckB4(I32+GEI32, 'var x=b4(0,0,0,0); var a=i4(-1,1,0,2); var b=i4(1,2,3,4);  x=ge(a,b)', [F, F, F, F]);
CheckB4(I32+GEI32, 'var x=b4(0,0,0,0); var a=i4(1,0,3,4);  var b=i4(1,1,7,0);  x=ge(a,b)', [T, F, F, T]);

const EQU32 = 'var eq = u4.equal';
const NEU32 = 'var ne = u4.notEqual';
const LTU32 = 'var lt = u4.lessThan;';
const LEU32 = 'var le = u4.lessThanOrEqual';
const GTU32 = 'var gt = u4.greaterThan;';
const GEU32 = 'var ge = u4.greaterThanOrEqual';

CheckB4(U32+EQU32, 'var x=b4(0,0,0,0); var a=u4(1,2,3,4);  var b=u4(-1,1,0,2); x=eq(a,b)', [F, F, F, F]);
CheckB4(U32+EQU32, 'var x=b4(0,0,0,0); var a=u4(-1,1,0,2); var b=u4(1,2,3,4);  x=eq(a,b)', [F, F, F, F]);
CheckB4(U32+EQU32, 'var x=b4(0,0,0,0); var a=u4(1,0,3,4);  var b=u4(1,1,7,0);  x=eq(a,b)', [T, F, F, F]);

CheckB4(U32+NEU32, 'var x=b4(0,0,0,0); var a=u4(1,2,3,4);  var b=u4(-1,1,0,2); x=ne(a,b)', [T, T, T, T]);
CheckB4(U32+NEU32, 'var x=b4(0,0,0,0); var a=u4(-1,1,0,2); var b=u4(1,2,3,4);  x=ne(a,b)', [T, T, T, T]);
CheckB4(U32+NEU32, 'var x=b4(0,0,0,0); var a=u4(1,0,3,4);  var b=u4(1,1,7,0);  x=ne(a,b)', [F, T, T, T]);

CheckB4(U32+LTU32, 'var x=b4(0,0,0,0); var a=u4(1,2,3,4);  var b=u4(-1,1,0,2); x=lt(a,b)', [T, F, F, F]);
CheckB4(U32+LTU32, 'var x=b4(0,0,0,0); var a=u4(-1,1,0,2); var b=u4(1,2,3,4);  x=lt(a,b)', [F, T, T, T]);
CheckB4(U32+LTU32, 'var x=b4(0,0,0,0); var a=u4(1,0,3,4);  var b=u4(1,1,7,0);  x=lt(a,b)', [F, T, T, F]);

CheckB4(U32+LEU32, 'var x=b4(0,0,0,0); var a=u4(1,2,3,4);  var b=u4(-1,1,0,2); x=le(a,b)', [T, F, F, F]);
CheckB4(U32+LEU32, 'var x=b4(0,0,0,0); var a=u4(-1,1,0,2); var b=u4(1,2,3,4);  x=le(a,b)', [F, T, T, T]);
CheckB4(U32+LEU32, 'var x=b4(0,0,0,0); var a=u4(1,0,3,4);  var b=u4(1,1,7,0);  x=le(a,b)', [T, T, T, F]);

CheckB4(U32+GTU32, 'var x=b4(0,0,0,0); var a=u4(1,2,3,4);  var b=u4(-1,1,0,2); x=gt(a,b)', [F, T, T, T]);
CheckB4(U32+GTU32, 'var x=b4(0,0,0,0); var a=u4(-1,1,0,2); var b=u4(1,2,3,4);  x=gt(a,b)', [T, F, F, F]);
CheckB4(U32+GTU32, 'var x=b4(0,0,0,0); var a=u4(1,0,3,4);  var b=u4(1,1,7,0);  x=gt(a,b)', [F, F, F, T]);

CheckB4(U32+GEU32, 'var x=b4(0,0,0,0); var a=u4(1,2,3,4);  var b=u4(-1,1,0,2); x=ge(a,b)', [F, T, T, T]);
CheckB4(U32+GEU32, 'var x=b4(0,0,0,0); var a=u4(-1,1,0,2); var b=u4(1,2,3,4);  x=ge(a,b)', [T, F, F, F]);
CheckB4(U32+GEU32, 'var x=b4(0,0,0,0); var a=u4(1,0,3,4);  var b=u4(1,1,7,0);  x=ge(a,b)', [T, F, F, T]);

const LTF32 = 'var lt=f4.lessThan;';
const LEF32 = 'var le=f4.lessThanOrEqual;';
const GTF32 = 'var gt=f4.greaterThan;';
const GEF32 = 'var ge=f4.greaterThanOrEqual;';
const EQF32 = 'var eq=f4.equal;';
const NEF32 = 'var ne=f4.notEqual;';

assertAsmTypeFail('glob', USE_ASM + F32 + "var lt=f4.lessThan; function f() {var x=f4(1,2,3,4); var y=f4(5,6,7,8); x=lt(x,y);} return f");

CheckB4(F32+LTF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=b4(0,0,0,0); x=lt(y,z)', [F, F, F, F]);
CheckB4(F32+LTF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=b4(0,0,0,0); x=lt(y,z)', [T, T, T, T]);
CheckB4(F32+LTF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=b4(0,0,0,0); x=lt(y,z)', [F, T, T, F]);
CheckB4(F32+LTF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=b4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=lt(y,z);', [F, F, F, F]);

CheckB4(F32+LEF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=b4(0,0,0,0); x=le(y,z)', [F, F, F, F]);
CheckB4(F32+LEF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=b4(0,0,0,0); x=le(y,z)', [T, T, T, T]);
CheckB4(F32+LEF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=b4(0,0,0,0); x=le(y,z)', [T, T, T, F]);
CheckB4(F32+LEF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=b4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=le(y,z);', [T, T, F, F]);

CheckB4(F32+EQF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=b4(0,0,0,0); x=eq(y,z)', [F, F, F, F]);
CheckB4(F32+EQF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=b4(0,0,0,0); x=eq(y,z)', [F, F, F, F]);
CheckB4(F32+EQF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=b4(0,0,0,0); x=eq(y,z)', [T, F, F, F]);
CheckB4(F32+EQF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=b4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=eq(y,z);', [T, T, F, F]);

CheckB4(F32+NEF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=b4(0,0,0,0); x=ne(y,z)', [T, T, T, T]);
CheckB4(F32+NEF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=b4(0,0,0,0); x=ne(y,z)', [T, T, T, T]);
CheckB4(F32+NEF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=b4(0,0,0,0); x=ne(y,z)', [F, T, T, T]);
CheckB4(F32+NEF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=b4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=ne(y,z);', [F, F, T, T]);

CheckB4(F32+GTF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=b4(0,0,0,0); x=gt(y,z)', [T, T, T, T]);
CheckB4(F32+GTF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=b4(0,0,0,0); x=gt(y,z)', [F, F, F, F]);
CheckB4(F32+GTF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=b4(0,0,0,0); x=gt(y,z)', [F, F, F, T]);
CheckB4(F32+GTF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=b4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=gt(y,z);', [F, F, F, F]);

CheckB4(F32+GEF32, 'var y=f4(1,2,3,4);  var z=f4(-1,1,0,2); var x=b4(0,0,0,0); x=ge(y,z)', [T, T, T, T]);
CheckB4(F32+GEF32, 'var y=f4(-1,1,0,2); var z=f4(1,2,3,4);  var x=b4(0,0,0,0); x=ge(y,z)', [F, F, F, F]);
CheckB4(F32+GEF32, 'var y=f4(1,0,3,4);  var z=f4(1,1,7,0);  var x=b4(0,0,0,0); x=ge(y,z)', [T, F, F, T]);
CheckB4(F32+GEF32 + 'const nan = glob.NaN; const fround=glob.Math.fround', 'var y=f4(0,0,0,0); var z=f4(0,0,0,0); var x=b4(0,0,0,0); y=f4(fround(0.0),fround(-0.0),fround(0.0),fround(nan)); z=f4(fround(-0.0),fround(0.0),fround(nan),fround(0.0)); x=ge(y,z);', [T, T, F, F]);

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + LTI32 + B32 + ANYB4 + 'function f(x){x=ci4(x); var y=i4(-1,0,4,5); var b=b4(0,0,0,0); b=lt(x,y); return anyt(b)|0;} return f'), this);
assertEq(f(SIMD.Int32x4(1,2,3,4)), 1);
assertEq(f(SIMD.Int32x4(1,2,4,5)), 0);
assertEq(f(SIMD.Int32x4(1,2,3,5)), 1);

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + LTI32 + B32 + ALLB4 + 'function f(x){x=ci4(x); var y=i4(-1,0,4,5); var b=b4(0,0,0,0); b=lt(x,y); return allt(b)|0;} return f'), this);
assertEq(f(SIMD.Int32x4(-2,-2,3,4)), 1);
assertEq(f(SIMD.Int32x4(1,2,4,5)), 0);
assertEq(f(SIMD.Int32x4(1,2,3,5)), 0);

// Conversions operators
const CVTIF = 'var cvt=f4.fromInt32x4;';
const CVTFI = 'var cvt=i4.fromFloat32x4;';
const CVTUF = 'var cvt=f4.fromUint32x4;';
const CVTFU = 'var cvt=u4.fromFloat32x4;';

assertAsmTypeFail('glob', USE_ASM + I32 + "var cvt=i4.fromInt32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + I32 + "var cvt=i4.fromUint32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + U32 + "var cvt=u4.fromInt32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + U32 + "var cvt=u4.fromUint32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + F32 + "var cvt=f4.fromFloat32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CVTIF + "function f() {var x=i4(1,2,3,4); x=cvt(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CVTIF + "function f() {var x=f4(1,2,3,4); x=cvt(x);} return f");

var f = asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CF32 + CI32 + CVTIF + 'function f(x){x=ci4(x); var y=f4(0,0,0,0); y=cvt(x); return cf4(y);} return f'), this);
assertEqX4(f(SIMD.Int32x4(1,2,3,4)), [1, 2, 3, 4]);
assertEqX4(f(SIMD.Int32x4(0,INT32_MIN,INT32_MAX,-1)), [0, Math.fround(INT32_MIN), Math.fround(INT32_MAX), -1]);

var f = asmLink(asmCompile('glob', USE_ASM + I32 + U32 + U32I32 + F32 + CF32 + CI32 + CVTUF +
                           'function f(x){x=ci4(x); var y=f4(0,0,0,0); y=cvt(u4i4(x)); return cf4(y);} return f'), this);
assertEqX4(f(SIMD.Int32x4(1,2,3,4)), [1, 2, 3, 4]);
assertEqX4(f(SIMD.Int32x4(0,INT32_MIN,INT32_MAX,-1)), [0, Math.fround(INT32_MAX+1), Math.fround(INT32_MAX), Math.fround(UINT32_MAX)]);

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + F32 + CF32 + CVTFI + 'function f(x){x=cf4(x); var y=i4(0,0,0,0); y=cvt(x); return ci4(y);} return f'), this);
assertEqX4(f(SIMD.Float32x4(1,2,3,4)), [1, 2, 3, 4]);
// Test that INT32_MIN (exactly representable as an float32) and the first
// integer representable as an float32 can be converted.
assertEqX4(f(SIMD.Float32x4(INT32_MIN, INT32_MAX - 64, -0, 0)), [INT32_MIN, INT32_MAX - 64, 0, 0].map(Math.fround));
// Test boundaries: first integer less than INT32_MIN and representable as a float32
assertThrowsInstanceOf(() => f(SIMD.Float32x4(INT32_MIN - 129, 0, 0, 0)), RangeError);
// INT_MAX + 1
assertThrowsInstanceOf(() => f(SIMD.Float32x4(Math.pow(2, 31), 0, 0, 0)), RangeError);
// Special values
assertThrowsInstanceOf(() => f(SIMD.Float32x4(NaN, 0, 0, 0)), RangeError);
assertThrowsInstanceOf(() => f(SIMD.Float32x4(Infinity, 0, 0, 0)), RangeError);
assertThrowsInstanceOf(() => f(SIMD.Float32x4(-Infinity, 0, 0, 0)), RangeError);

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + U32 + I32U32 + F32 + CF32 + CVTFU +
                           'function f(x){x=cf4(x); var y=u4(0,0,0,0); y=cvt(x); return ci4(i4u4(y));} return f'), this);
assertEqX4(f(SIMD.Float32x4(1,2,3,4)), [1, 2, 3, 4]);
// TODO: Test negative numbers > -1. They should truncate to 0. See https://github.com/tc39/ecmascript_simd/issues/315
assertEqX4(SIMD.Uint32x4.fromInt32x4Bits(f(SIMD.Float32x4(0xffffff00, INT32_MAX+1, -0, 0))),
           [0xffffff00, INT32_MAX+1, 0, 0].map(Math.fround));
// Test boundaries: -1 or less.
assertThrowsInstanceOf(() => f(SIMD.Float32x4(-1, 0, 0, 0)), RangeError);
assertThrowsInstanceOf(() => f(SIMD.Float32x4(Math.pow(2, 32), 0, 0, 0)), RangeError);
// Special values
assertThrowsInstanceOf(() => f(SIMD.Float32x4(NaN, 0, 0, 0)), RangeError);
assertThrowsInstanceOf(() => f(SIMD.Float32x4(Infinity, 0, 0, 0)), RangeError);
assertThrowsInstanceOf(() => f(SIMD.Float32x4(-Infinity, 0, 0, 0)), RangeError);

// Cast operators
const CVTIFB = 'var cvt=f4.fromInt32x4Bits;';
const CVTFIB = 'var cvt=i4.fromFloat32x4Bits;';

var cast = (function() {
    var i32 = new Int32Array(1);
    var f32 = new Float32Array(i32.buffer);

    function fromInt32Bits(x) {
        i32[0] = x;
        return f32[0];
    }

    function fromFloat32Bits(x) {
        f32[0] = x;
        return i32[0];
    }

    return {
        fromInt32Bits,
        fromFloat32Bits
    }
})();

assertAsmTypeFail('glob', USE_ASM + I32 + "var cvt=i4.fromInt32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + F32 + "var cvt=f4.fromFloat32x4; return {}");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CVTIFB + "function f() {var x=i4(1,2,3,4); x=cvt(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CVTIFB + "function f() {var x=f4(1,2,3,4); x=cvt(x);} return f");

var f = asmLink(asmCompile('glob', USE_ASM + I32 + F32 + CVTIFB + CF32 + CI32 + 'function f(x){x=ci4(x); var y=f4(0,0,0,0); y=cvt(x); return cf4(y);} return f'), this);
assertEqX4(f(SIMD.Int32x4(1,2,3,4)), [1, 2, 3, 4].map(cast.fromInt32Bits));
assertEqX4(f(SIMD.Int32x4(0,INT32_MIN,INT32_MAX,-1)), [0, INT32_MIN, INT32_MAX, -1].map(cast.fromInt32Bits));

var f = asmLink(asmCompile('glob', USE_ASM + I32 + F32 + F32A + CVTIFB + CF32 + CI32 + 'function f(x){x=ci4(x); var y=f4(0,0,0,0); var z=f4(1,1,1,1); y=cvt(x); y=f4a(y, z); return cf4(y)} return f'), this);
assertEqX4(f(SIMD.Int32x4(1,2,3,4)), [1, 2, 3, 4].map(cast.fromInt32Bits).map((x) => x+1));
assertEqX4(f(SIMD.Int32x4(0,INT32_MIN,INT32_MAX,-1)), [0, INT32_MIN, INT32_MAX, -1].map(cast.fromInt32Bits).map((x) => x+1));

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + F32 + CF32 + CVTFIB + 'function f(x){x=cf4(x); var y=i4(0,0,0,0); y=cvt(x); return ci4(y);} return f'), this);
assertEqX4(f(SIMD.Float32x4(1,2,3,4)), [1, 2, 3, 4].map(cast.fromFloat32Bits));
assertEqX4(f(SIMD.Float32x4(-0,NaN,+Infinity,-Infinity)), [-0, NaN, +Infinity, -Infinity].map(cast.fromFloat32Bits));

var f = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + F32 + CF32 + I32A + CVTFIB + 'function f(x){x=cf4(x); var y=i4(0,0,0,0); var z=i4(1,1,1,1); y=cvt(x); y=i4a(y,z); return ci4(y);} return f'), this);
assertEqX4(f(SIMD.Float32x4(1,2,3,4)), [1, 2, 3, 4].map(cast.fromFloat32Bits).map((x) => x+1));
assertEqX4(f(SIMD.Float32x4(-0,NaN,+Infinity,-Infinity)), [-0, NaN, +Infinity, -Infinity].map(cast.fromFloat32Bits).map((x) => x+1));

// Bitwise ops
const ANDI32 = 'var andd=i4.and;';
const ORI32 = 'var orr=i4.or;';
const XORI32 = 'var xorr=i4.xor;';

CheckI4(ANDI32, 'var x=i4(42,1337,-1,13); var y=i4(2, 4, 7, 15); x=andd(x,y)', [42 & 2, 1337 & 4, -1 & 7, 13 & 15]);
CheckI4(ORI32, ' var x=i4(42,1337,-1,13); var y=i4(2, 4, 7, 15); x=orr(x,y)',  [42 | 2, 1337 | 4, -1 | 7, 13 | 15]);
CheckI4(XORI32, 'var x=i4(42,1337,-1,13); var y=i4(2, 4, 7, 15); x=xorr(x,y)', [42 ^ 2, 1337 ^ 4, -1 ^ 7, 13 ^ 15]);

const ANDU32 = 'var andd=u4.and;';
const ORU32 = 'var orr=u4.or;';
const XORU32 = 'var xorr=u4.xor;';

CheckU4(ANDU32, 'var x=u4(42,1337,-1,13); var y=u4(2, 4, 7, 15); x=andd(x,y)', [42 & 2, 1337 & 4, (-1 & 7) >>> 0, 13 & 15]);
CheckU4(ORU32, ' var x=u4(42,1337,-1,13); var y=u4(2, 4, 7, 15); x=orr(x,y)',  [42 | 2, 1337 | 4, (-1 | 7) >>> 0, 13 | 15]);
CheckU4(XORU32, 'var x=u4(42,1337,-1,13); var y=u4(2, 4, 7, 15); x=xorr(x,y)', [42 ^ 2, 1337 ^ 4, (-1 ^ 7) >>> 0, 13 ^ 15]);

const ANDB32 = 'var andd=b4.and;';
const ORB32 = 'var orr=b4.or;';
const XORB32 = 'var xorr=b4.xor;';

CheckB4(ANDB32, 'var x=b4(1,0,1,0); var y=b4(1,1,0,0); x=andd(x,y)', [true, false, false, false]);
CheckB4(ORB32, ' var x=b4(1,0,1,0); var y=b4(1,1,0,0); x=orr(x,y)',  [true, true, true, false]);
CheckB4(XORB32, 'var x=b4(1,0,1,0); var y=b4(1,1,0,0); x=xorr(x,y)', [false, true, true, false]);

// No bitwise ops on Float32x4.
const ANDF32 = 'var andd=f4.and;';
const ORF32 = 'var orr=f4.or;';
const XORF32 = 'var xorr=f4.xor;';
const NOTF32 = 'var nott=f4.not;';

assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + ANDF32 + 'function f() {var x=f4(42, 13.37,-1.42, 23.10); var y=f4(19.89, 2.4, 8.15, 16.36); x=andd(x,y);} return f');
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + ORF32 + 'function f() {var x=f4(42, 13.37,-1.42, 23.10); var y=f4(19.89, 2.4, 8.15, 16.36); x=orr(x,y);} return f');
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + XORF32 + 'function f() {var x=f4(42, 13.37,-1.42, 23.10); var y=f4(19.89, 2.4, 8.15, 16.36); x=xorr(x,y);} return f');
assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + NOTF32 + 'function f() {var x=f4(42, 13.37,-1.42, 23.10); x=nott(x);} return f');

// Logical ops
const LSHI = 'var lsh=i4.shiftLeftByScalar;'
const RSHI = 'var rsh=i4.shiftRightByScalar;'

assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + F32 + FROUND + LSHI + "function f() {var x=f4(1,2,3,4); return ci4(lsh(x,f32(42)));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + F32 + FROUND + LSHI + "function f() {var x=f4(1,2,3,4); return ci4(lsh(x,42));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + FROUND + LSHI + "function f() {var x=i4(1,2,3,4); return ci4(lsh(x,42.0));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + CI32 + FROUND + LSHI + "function f() {var x=i4(1,2,3,4); return ci4(lsh(x,f32(42)));} return f");

var input = 'i4(0, 1, ' + INT32_MIN + ', ' + INT32_MAX + ')';
var vinput = [0, 1, INT32_MIN, INT32_MAX];

function Lsh(i) { return function(x) { return (x << (i & 31)) | 0 } }
function Rsh(i) { return function(x) { return (x >> (i & 31)) | 0 } }

var asmLsh = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + LSHI + 'function f(x, y){x=x|0;y=y|0; var v=' + input + ';return ci4(lsh(v, x+y))} return f;'), this)
var asmRsh = asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + RSHI + 'function f(x, y){x=x|0;y=y|0; var v=' + input + ';return ci4(rsh(v, x+y))} return f;'), this)

for (var i = 1; i < 64; i++) {
    CheckI4(LSHI,  'var x=' + input + '; x=lsh(x, ' + i + ')',   vinput.map(Lsh(i)));
    CheckI4(RSHI,  'var x=' + input + '; x=rsh(x, ' + i + ')',   vinput.map(Rsh(i)));

    assertEqX4(asmLsh(i, 3),  vinput.map(Lsh(i + 3)));
    assertEqX4(asmRsh(i, 3),  vinput.map(Rsh(i + 3)));
}

// Same thing for Uint32x4.
const LSHU = 'var lsh=u4.shiftLeftByScalar;'
const RSHU = 'var rsh=u4.shiftRightByScalar;'

input = 'u4(0, 1, 0x80008000, ' + INT32_MAX + ')';
vinput = [0, 1, 0x80008000, INT32_MAX];

function uLsh(i) { return function(x) { return (x << (i & 31)) >>> 0 } }
function uRsh(i) { return function(x) { return (x >>> (i & 31)) } }

// Need to bitcast to Int32x4 before returning result.
asmLsh = asmLink(asmCompile('glob', USE_ASM + U32 + CU32 + LSHU + I32 + CI32 + I32U32 +
                            'function f(x, y){x=x|0;y=y|0; var v=' + input + ';return ci4(i4u4(lsh(v, x+y)));} return f;'), this)
asmRsh = asmLink(asmCompile('glob', USE_ASM + U32 + CU32 + RSHU + I32 + CI32 + I32U32 +
                            'function f(x, y){x=x|0;y=y|0; var v=' + input + ';return ci4(i4u4(rsh(v, x+y)));} return f;'), this)

for (var i = 1; i < 64; i++) {
    // Constant shifts.
    CheckU4(LSHU,  'var x=' + input + '; x=lsh(x, ' + i + ')', vinput.map(uLsh(i)));
    CheckU4(RSHU,  'var x=' + input + '; x=rsh(x, ' + i + ')', vinput.map(uRsh(i)));

    // Dynamically computed shifts. The asm function returns a Int32x4.
    assertEqX4(SIMD.Uint32x4.fromInt32x4Bits(asmLsh(i, 3)), vinput.map(uLsh(i + 3)));
    assertEqX4(SIMD.Uint32x4.fromInt32x4Bits(asmRsh(i, 3)), vinput.map(uRsh(i + 3)));
}

// Select
const I32SEL = 'var i4sel = i4.select;'
const U32SEL = 'var u4sel = u4.select;'
const F32SEL = 'var f4sel = f4.select;'

assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var x=f4(1,2,3,4); return ci4(i4sel(x,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var m=f4(1,2,3,4); var x=i4(1,2,3,4); return ci4(i4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var m=f4(1,2,3,4); var x=f4(1,2,3,4); return ci4(i4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var m=i4(1,2,3,4); var x=f4(1,2,3,4); return ci4(i4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var m=i4(1,2,3,4); var x=i4(1,2,3,4); return ci4(i4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var m=b4(1,2,3,4); var x=f4(1,2,3,4); return ci4(i4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var m=b4(1,2,3,4); var x=f4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var m=b4(1,2,3,4); var x=i4(1,2,3,4); var y=f4(5,6,7,8); return ci4(i4sel(m,x,y));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var m=b4(1,2,3,4); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return ci4(i4sel(m,x,y));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + B32 + CI32 + I32SEL + "function f() {var m=b4(1,2,3,4); var x=i4(1,2,3,4); var y=b4(5,6,7,8); return ci4(i4sel(m,x,y));} return f");

assertAsmTypeFail('glob', USE_ASM + F32 + CF32 + F32SEL + "function f() {var m=f4(1,2,3,4); return cf4(f4sel(x,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=f4(1,2,3,4); var x=i4(1,2,3,4); return cf4(f4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=f4(1,2,3,4); var x=f4(1,2,3,4); return cf4(f4sel(m,x,x));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(1,2,3,4); var x=f4(1,2,3,4); var y=i4(5,6,7,8); return cf4(f4sel(m,x,y));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + CF32 + F32SEL + "function f() {var m=i4(1,2,3,4); var x=i4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y));} return f");

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + B32 + CI32 + I32SEL + "function f() {var m=b4(0,0,0,0); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 6, 7, 8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + B32 + CI32 + I32SEL + "function f() {var m=b4(1,1,1,1); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + B32 + CI32 + I32SEL + "function f() {var m=b4(0,1,0,1); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 2, 7, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + B32 + CI32 + I32SEL + "function f() {var m=b4(0,0,1,1); var x=i4(1,2,3,4); var y=i4(5,6,7,8); return ci4(i4sel(m,x,y)); } return f"), this)(), [5, 6, 3, 4]);

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + B32 + F32 + CF32 + F32SEL + "function f() {var m=b4(0,0,0,0); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 6, 7, 8]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + B32 + F32 + CF32 + F32SEL + "function f() {var m=b4(1,1,1,1); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + B32 + F32 + CF32 + F32SEL + "function f() {var m=b4(0,1,0,1); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 2, 7, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + B32 + F32 + CF32 + F32SEL + "function f() {var m=b4(0,0,1,1); var x=f4(1,2,3,4); var y=f4(5,6,7,8); return cf4(f4sel(m,x,y)); } return f"), this)(), [5, 6, 3, 4]);

CheckU4(B32 + U32SEL, "var m=b4(0,0,0,0); var x=u4(1,2,3,4); var y=u4(5,6,7,8); x=u4sel(m,x,y);", [5, 6, 7, 8]);
CheckU4(B32 + U32SEL, "var m=b4(1,1,1,1); var x=u4(1,2,3,4); var y=u4(5,6,7,8); x=u4sel(m,x,y);", [1, 2, 3, 4]);
CheckU4(B32 + U32SEL, "var m=b4(0,1,0,1); var x=u4(1,2,3,4); var y=u4(5,6,7,8); x=u4sel(m,x,y);", [5, 2, 7, 4]);
CheckU4(B32 + U32SEL, "var m=b4(0,0,1,1); var x=u4(1,2,3,4); var y=u4(5,6,7,8); x=u4sel(m,x,y);", [5, 6, 3, 4]);

// Splat
const I32SPLAT = 'var splat=i4.splat;'
const U32SPLAT = 'var splat=u4.splat;'
const F32SPLAT = 'var splat=f4.splat;'
const B32SPLAT = 'var splat=b4.splat;'

assertAsmTypeFail('glob', USE_ASM + I32 + F32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); var p=f4(1.,2.,3.,4.); p=splat(f32(1));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); m=splat(1, 2)} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); m=splat()} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); m=splat(m);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + "function f() {var m=i4(1,2,3,4); m=splat(1.0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + I32SPLAT + FROUND + "function f() {var m=i4(1,2,3,4); m=splat(f32(1.0));} return f");

assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32SPLAT + 'function f(){return ci4(splat(42));} return f'), this)(), [42, 42, 42, 42]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + B32 + CB32 + B32SPLAT + 'function f(){return cb4(splat(42));} return f'), this)(), [true, true, true, true]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + B32 + CB32 + B32SPLAT + 'function f(){return cb4(splat(0));} return f'), this)(), [false, false, false, false]);
CheckU4(B32 + U32SPLAT, "var x=u4(1,2,3,4); x=splat(0);", [0, 0, 0, 0]);
CheckU4(B32 + U32SPLAT, "var x=u4(1,2,3,4); x=splat(0xaabbccdd);", [0xaabbccdd, 0xaabbccdd, 0xaabbccdd, 0xaabbccdd]);

const l33t = Math.fround(13.37);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(f32(1)));} return f'), this)(), [1, 1, 1, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(1.0));} return f'), this)(), [1, 1, 1, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(f32(1 >>> 0)));} return f'), this)(), [1, 1, 1, 1]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(f32(13.37)));} return f'), this)(), [l33t, l33t, l33t, l33t]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(13.37));} return f'), this)(), [l33t, l33t, l33t, l33t]);

var i32view = new Int32Array(heap);
var f32view = new Float32Array(heap);
i32view[0] = 42;
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + I32 + CI32 + I32SPLAT + 'var i32=new glob.Int32Array(heap); function f(){return ci4(splat(i32[0]));} return f'), this, {}, heap)(), [42, 42, 42, 42]);
f32view[0] = 42;
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + F32 + CF32 + F32SPLAT + 'var f32=new glob.Float32Array(heap); function f(){return cf4(splat(f32[0]));} return f'), this, {}, heap)(), [42, 42, 42, 42]);
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', USE_ASM + F32 + CF32 + F32SPLAT + FROUND + 'function f(){return cf4(splat(f32(1) + f32(2)));} return f'), this, {}, heap)(), [3, 3, 3, 3]);

// Dead code
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + 'function f(){var x=i4(1,2,3,4); return ci4(x); x=i4(5,6,7,8); return ci4(x);} return f'), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + EXTI4 + 'function f(){var x=i4(1,2,3,4); var c=0; return ci4(x); c=e(x,0)|0; return ci4(x);} return f'), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32A + 'function f(){var x=i4(1,2,3,4); var c=0; return ci4(x); x=i4a(x,x); return ci4(x);} return f'), this)(), [1, 2, 3, 4]);
assertEqX4(asmLink(asmCompile('glob', USE_ASM + I32 + CI32 + I32S + 'function f(){var x=i4(1,2,3,4); var c=0; return ci4(x); x=i4s(x,x); return ci4(x);} return f'), this)(), [1, 2, 3, 4]);

// Swizzle
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x, -1, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x, 4, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x, 0.0, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); x=swizzle(x, 0, 0, 0, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var swizzle=i4.swizzle; function f() {var x=i4(1,2,3,4); var y=42; x=swizzle(x, 0, 0, 0, y);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "var swizzle=i4.swizzle; function f() {var x=f4(1,2,3,4); x=swizzle(x, 0, 0, 0, 0);} return f");

function swizzle(arr, lanes) {
    return [arr[lanes[0]], arr[lanes[1]], arr[lanes[2]], arr[lanes[3]]];
}

var before = Date.now();
for (var i = 0; i < Math.pow(4, 4); i++) {
    var lanes = [i & 3, (i >> 2) & 3, (i >> 4) & 3, (i >> 6) & 3];
    CheckI4('var swizzle=i4.swizzle;', 'var x=i4(1,2,3,4); x=swizzle(x, ' + lanes.join(',') + ')', swizzle([1,2,3,4], lanes));
    CheckU4('var swizzle=u4.swizzle;', 'var x=u4(1,2,3,4); x=swizzle(x, ' + lanes.join(',') + ')', swizzle([1,2,3,4], lanes));
    CheckF4('var swizzle=f4.swizzle;', 'var x=f4(1,2,3,4); x=swizzle(x, ' + lanes.join(',') + ')', swizzle([1,2,3,4], lanes));
}
DEBUG && print('time for checking all swizzles:', Date.now() - before);

// Shuffle
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, y, -1, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, y, 8, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, y, 0.0, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, 0, 0, 0, 0, 0);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); x=shuffle(x, y, 0, 0, 0, x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var shuffle=i4.shuffle; function f() {var x=i4(1,2,3,4); var y=i4(1,2,3,4); var z=42; x=shuffle(x, y, 0, 0, 0, z);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + F32 + "var shuffle=i4.shuffle; function f() {var x=f4(1,2,3,4); x=shuffle(x, x, 0, 0, 0, 0);} return f");

function shuffle(lhs, rhs, lanes) {
    return [(lanes[0] < 4 ? lhs : rhs)[lanes[0] % 4],
            (lanes[1] < 4 ? lhs : rhs)[lanes[1] % 4],
            (lanes[2] < 4 ? lhs : rhs)[lanes[2] % 4],
            (lanes[3] < 4 ? lhs : rhs)[lanes[3] % 4]];
}

before = Date.now();

const LANE_SELECTORS = [
    // Four of lhs or four of rhs, equivalent to swizzle
    [0, 1, 2, 3],
    [4, 5, 6, 7],
    [0, 2, 3, 1],
    [4, 7, 4, 6],
    // One of lhs, three of rhs
    [0, 4, 5, 6],
    [4, 0, 5, 6],
    [4, 5, 0, 6],
    [4, 5, 6, 0],
    // Two of lhs, two of rhs
    //      in one shufps
    [1, 2, 4, 5],
    [4, 5, 1, 2],
    //      in two shufps
    [7, 0, 5, 2],
    [0, 7, 5, 2],
    [0, 7, 2, 5],
    [7, 0, 2, 5],
    // Three of lhs, one of rhs
    [7, 0, 1, 2],
    [0, 7, 1, 2],
    [0, 1, 7, 2],
    [0, 1, 2, 7],
    // Impl-specific special cases for swizzle
    [2, 3, 2, 3],
    [0, 1, 0, 1],
    [0, 0, 1, 1],
    [2, 2, 3, 3],
    // Impl-specific special cases for shuffle (case and swapped case)
    [2, 3, 6, 7], [6, 7, 2, 3],
    [0, 1, 4, 5], [4, 5, 0, 1],
    [0, 4, 1, 5], [4, 0, 5, 1],
    [2, 6, 3, 7], [6, 2, 7, 3],
    [4, 1, 2, 3], [0, 5, 6, 7],
    // Insert one element from rhs into lhs keeping other elements unchanged
    [7, 1, 2, 3],
    [0, 7, 2, 3],
    [0, 1, 7, 2],
    // These are effectively vector selects
    [0, 5, 2, 3],
    [0, 1, 6, 3],
    [4, 5, 2, 3],
    [4, 1, 6, 3]
];

for (var lanes of LANE_SELECTORS) {
    CheckI4('var shuffle=i4.shuffle;', 'var x=i4(1,2,3,4); var y=i4(5,6,7,8); x=shuffle(x, y, ' + lanes.join(',') + ')', shuffle([1,2,3,4], [5,6,7,8], lanes));
    CheckU4('var shuffle=u4.shuffle;', 'var x=u4(1,2,3,4); var y=u4(5,6,7,8); x=shuffle(x, y, ' + lanes.join(',') + ')', shuffle([1,2,3,4], [5,6,7,8], lanes));
    CheckF4('var shuffle=f4.shuffle;', 'var x=f4(1,2,3,4); var y=f4(5,6,7,8); x=shuffle(x, y, ' + lanes.join(',') + ')', shuffle([1,2,3,4], [5,6,7,8], lanes));
}
DEBUG && print('time for checking all shuffles:', Date.now() - before);

// 3. Function calls
// 3.1. No math builtins
assertAsmTypeFail('glob', USE_ASM + I32 + "var fround=glob.Math.fround; function f() {var x=i4(1,2,3,4); return +fround(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var sin=glob.Math.sin; function f() {var x=i4(1,2,3,4); return +sin(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var ceil=glob.Math.ceil; function f() {var x=i4(1,2,3,4); return +ceil(x);} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var pow=glob.Math.pow; function f() {var x=i4(1,2,3,4); return +pow(1.0, x);} return f");

assertAsmTypeFail('glob', USE_ASM + I32 + "var fround=glob.Math.fround; function f() {var x=i4(1,2,3,4); x=i4(fround(3));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var sin=glob.Math.sin; function f() {var x=i4(1,2,3,4); x=i4(sin(3.0));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var ceil=glob.Math.sin; function f() {var x=i4(1,2,3,4); x=i4(ceil(3.0));} return f");
assertAsmTypeFail('glob', USE_ASM + I32 + "var pow=glob.Math.pow; function f() {var x=i4(1,2,3,4); x=i4(pow(1.0, 2.0));} return f");

// 3.2. FFI calls
// Can't pass SIMD arguments to FFI
assertAsmTypeFail('glob', 'ffi', USE_ASM + I32 + "var func=ffi.func; function f() {var x=i4(1,2,3,4); func(x);} return f");
assertAsmTypeFail('glob', 'ffi', USE_ASM + U32 + "var func=ffi.func; function f() {var x=u4(1,2,3,4); func(x);} return f");
assertAsmTypeFail('glob', 'ffi', USE_ASM + F32 + "var func=ffi.func; function f() {var x=f4(1,2,3,4); func(x);} return f");
assertAsmTypeFail('glob', 'ffi', USE_ASM + B32 + "var func=ffi.func; function f() {var x=b4(1,2,3,4); func(x);} return f");

// Can't have FFI return SIMD values
assertAsmTypeFail('glob', 'ffi', USE_ASM + I32 + "var func=ffi.func; function f() {var x=i4(1,2,3,4); x=i4(func());} return f");
assertAsmTypeFail('glob', 'ffi', USE_ASM + U32 + "var func=ffi.func; function f() {var x=u4(1,2,3,4); x=i4(func());} return f");
assertAsmTypeFail('glob', 'ffi', USE_ASM + F32 + "var func=ffi.func; function f() {var x=f4(1,2,3,4); x=f4(func());} return f");
assertAsmTypeFail('glob', 'ffi', USE_ASM + B32 + "var func=ffi.func; function f() {var x=b4(1,2,3,4); x=b4(func());} return f");

// 3.3 Internal calls
// asm.js -> asm.js
// Retrieving values from asm.js
var code = USE_ASM + I32 + CI32 + I32A + EXTI4 + `
    var check = ffi.check;

    function g() {
        var i = 0;
        var y = i4(0,0,0,0);
        var tmp = i4(0,0,0,0); var z = i4(1,1,1,1);
        var w = i4(5,5,5,5);
        for (; (i|0) < 30; i = i + 1 |0)
            y = i4a(z, y);
        y = i4a(w, y);
        check(e(y,0) | 0, e(y,1) | 0, e(y,2) | 0, e(y,3) | 0);
        return ci4(y);
    }

    function f(x) {
        x = ci4(x);
        var y = i4(0,0,0,0);
        y = ci4(g());
        check(e(y,0) | 0, e(y,1) | 0, e(y,2) | 0, e(y,3) | 0);
        return ci4(x);
    }
    return f;
`;

var v4 = SIMD.Int32x4(1,2,3,4);
function check(x, y, z, w) {
    assertEq(x, 35);
    assertEq(y, 35);
    assertEq(z, 35);
    assertEq(w, 35);
}
var ffi = {check};
assertEqX4(asmLink(asmCompile('glob', 'ffi', code), this, ffi)(v4), [1,2,3,4]);

// Passing arguments from asm.js to asm.js
var code = USE_ASM + I32 + CI32 + I32A + EXTI4 + `
    var assertEq = ffi.assertEq;

    function internal([args]) {
        [coerc]
        assertEq(e([last],0) | 0, [i] | 0);
        assertEq(e([last],1) | 0, [i] + 1 |0);
        assertEq(e([last],2) | 0, [i] + 2 |0);
        assertEq(e([last],3) | 0, [i] + 3 |0);
    }

    function external() {
        [decls]
        internal([args]);
    }
    return external;
`;

var ffi = {assertEq};
var args = '';
var decls = '';
var coerc = '';
for (var i = 1; i < 10; ++i) {
    var j = i;
    args += ((i > 1) ? ', ':'') + 'x' + i;
    decls += 'var x' + i + ' = i4(' + j++ + ', ' + j++ + ', ' + j++ + ', ' + j++ + ');\n';
    coerc += 'x' + i + ' = ci4(x' + i + ');\n';
    last = 'x' + i;
    var c = code.replace(/\[args\]/g, args)
                .replace(/\[last\]/g, last)
                .replace(/\[decls\]/i, decls)
                .replace(/\[coerc\]/i, coerc)
                .replace(/\[i\]/g, i);
    asmLink(asmCompile('glob', 'ffi', c), this, ffi)();
}

// Bug 1240524
assertAsmTypeFail(USE_ASM + B32 + 'var x = b4(0, 0, 0, 0); frd(x);');

// Passing boolean results to extern functions.
// Verify that these functions are typed correctly.
function isone(x) { return (x===1)|0 }
var f = asmLink(asmCompile('glob', 'ffi', USE_ASM + B32 + CB32 + ANYB4 + 'var isone=ffi.isone; function f(i) { i=cb4(i); return isone(anyt(i)|0)|0; } return f'), this, {isone:isone});
assertEq(f(SIMD.Bool32x4(0,0,1,0)), 1)
assertEq(f(SIMD.Bool32x4(0,0,0,0)), 0)
assertAsmTypeFail('glob', 'ffi', USE_ASM + B32 + CB32 + ANYB4 + 'var isone=ffi.isone; function f(i) { i=cb4(i); return isone(anyt(i))|0; } return f');

var f = asmLink(asmCompile('glob', 'ffi', USE_ASM + B32 + CB32 + ALLB4 + 'var isone=ffi.isone; function f(i) { i=cb4(i); return isone(allt(i)|0)|0; } return f'), this, {isone:isone});
assertEq(f(SIMD.Bool32x4(1,1,1,1)), 1)
assertEq(f(SIMD.Bool32x4(0,1,0,0)), 0)
assertAsmTypeFail('glob', 'ffi', USE_ASM + B32 + CB32 + ALLB4 + 'var isone=ffi.isone; function f(i) { i=cb4(i); return isone(allt(i))|0; } return f');

var f = asmLink(asmCompile('glob', 'ffi', USE_ASM + B32 + CB32 + EXTB4 + 'var isone=ffi.isone; function f(i) { i=cb4(i); return isone(e(i,2)|0)|0; } return f'), this, {isone:isone});
assertEq(f(SIMD.Bool32x4(1,1,1,1)), 1)
assertEq(f(SIMD.Bool32x4(0,1,0,0)), 0)
assertAsmTypeFail('glob', 'ffi', USE_ASM + B32 + CB32 + EXTB4 + 'var isone=ffi.isone; function f(i) { i=cb4(i); return isone(e(i,2))|0; } return f');

// Stress-test for register spilling code and stack depth checks
var code = `
    "use asm";
    var i4 = glob.SIMD.Int32x4;
    var i4a = i4.add;
    var e = i4.extractLane;
    var assertEq = ffi.assertEq;
    function g() {
        var x = i4(1,2,3,4);
        var y = i4(2,3,4,5);
        var z = i4(0,0,0,0);
        z = i4a(x, y);
        assertEq(e(z,0) | 0, 3);
        assertEq(e(z,1) | 0, 5);
        assertEq(e(z,2) | 0, 7);
        assertEq(e(z,3) | 0, 9);
    }
    return g
`
asmLink(asmCompile('glob', 'ffi', code), this, assertEqFFI)();

(function() {
    var code = `
        "use asm";
        var i4 = glob.SIMD.Int32x4;
        var i4a = i4.add;
        var e = i4.extractLane;
        var assertEq = ffi.assertEq;
        var one = ffi.one;

        // Function call with arguments on the stack (1 on x64, 3 on x86)
        function h(x1, x2, x3, x4, x5, x6, x7) {
            x1=x1|0
            x2=x2|0
            x3=x3|0
            x4=x4|0
            x5=x5|0
            x6=x6|0
            x7=x7|0
            return x1 + x2 |0
        }

        function g() {
            var x = i4(1,2,3,4);
            var y = i4(2,3,4,5);
            var z = i4(0,0,0,0);
            var w = 1;
            z = i4a(x, y);
            w = w + (one() | 0) | 0;
            assertEq(e(z,0) | 0, 3);
            assertEq(e(z,1) | 0, 5);
            assertEq(e(z,2) | 0, 7);
            assertEq(e(z,3) | 0, 9);
            h(1, 2, 3, 4, 42, 42, 42)|0
            return w | 0;
        }
        return g
    `;

    asmLink(asmCompile('glob', 'ffi', code), this, {assertEq: assertEq, one: () => 1})();
})();

// Function calls with mixed arguments on the stack (SIMD and scalar). In the
// worst case (x64), we have 6 int arg registers and 8 float registers.
(function() {
    var code = `
        "use asm";
        var i4 = glob.SIMD.Int32x4;
        var e = i4.extractLane;
        var ci4 = i4.check;
        function h(
            // In registers:
            gpr1, gpr2, gpr3, gpr4, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8,
            // On the stack:
            sint1, ssimd1, sdouble1, ssimd2, sint2, sint3, sint4, ssimd3, sdouble2
            )
        {
            gpr1=gpr1|0;
            gpr2=gpr2|0;
            gpr3=gpr3|0;
            gpr4=gpr4|0;

            xmm1=+xmm1;
            xmm2=+xmm2;
            xmm3=+xmm3;
            xmm4=+xmm4;
            xmm5=+xmm5;
            xmm6=+xmm6;
            xmm7=+xmm7;
            xmm8=+xmm8;

            sint1=sint1|0;
            ssimd1=ci4(ssimd1);
            sdouble1=+sdouble1;
            ssimd2=ci4(ssimd2);
            sint2=sint2|0;
            sint3=sint3|0;
            sint4=sint4|0;
            ssimd3=ci4(ssimd3);
            sdouble2=+sdouble2;

            return (e(ssimd1,0)|0) + (e(ssimd2,1)|0) + (e(ssimd3,2)|0) + sint2 + gpr3 | 0;
        }

        function g() {
            var simd1 = i4(1,2,3,4);
            var simd2 = i4(5,6,7,8);
            var simd3 = i4(9,10,11,12);
            return h(1, 2, 3, 4,
                     1., 2., 3., 4., 5., 6., 7., 8.,
                     5, simd1, 9., simd2, 6, 7, 8, simd3, 10.) | 0;
        }
        return g
    `;

    assertEq(asmLink(asmCompile('glob', 'ffi', code), this)(), 1 + 6 + 11 + 6 + 3);
})();

// Check that the interrupt callback doesn't erase high components of simd
// registers:

// WARNING: must be the last test in this file
(function() {
    var iters = 2000000;
    var code = `
    "use asm";
    var i4 = glob.SIMD.Int32x4;
    var i4a = i4.add;
    var ci4 = i4.check;
    function _() {
        var i = 0;
        var n = i4(0,0,0,0);
        var one = i4(1,1,1,1);
        for (; (i>>>0) < ` + iters + `; i=(i+1)>>>0) {
            n = i4a(n, one);
        }
        return ci4(n);
    }
    return _;`;
    // This test relies on the fact that setting the timeout will call the
    // interrupt callback at fixed intervals, even before the timeout.
    timeout(1000);
    var x4 = asmLink(asmCompile('glob', code), this)();
    assertEq(SIMD.Int32x4.extractLane(x4,0), iters);
    assertEq(SIMD.Int32x4.extractLane(x4,1), iters);
    assertEq(SIMD.Int32x4.extractLane(x4,2), iters);
    assertEq(SIMD.Int32x4.extractLane(x4,3), iters);
})();

} catch(e) {
    print('Stack:', e.stack)
    print('Error:', e)
    throw e;
}
