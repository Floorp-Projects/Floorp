// |jit-test| test-also-noasmjs
load(libdir + "asm.js");
load(libdir + "asserts.js");

// Set to true to see more JS debugging spew
const DEBUG = false;

if (!isSimdAvailable() || typeof SIMD === 'undefined') {
    DEBUG && print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

const INT32_MAX = Math.pow(2, 31) - 1;
const INT32_MIN = INT32_MAX + 1 | 0;

function assertEqX4(real, expected, assertFunc) {
    if (typeof assertFunc === 'undefined')
        assertFunc = assertEq;

    assertFunc(real.x, expected[0]);
    assertFunc(real.y, expected[1]);
    assertFunc(real.z, expected[2]);
    assertFunc(real.w, expected[3]);
}

// Load / Store
var IMPORTS = USE_ASM + 'var H=new glob.Uint8Array(heap); var i4=glob.SIMD.int32x4; var load=i4.load; var store=i4.store;';

//      Bad number of args
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load();} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load(3);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load(3, 4, 5);} return f");

//      Bad type of args
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load(3, 5);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load(H, 5.0);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){var i=0.;load(H, i);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "var H2=new glob.Int32Array(heap); function f(){var i=0;load(H2, i)} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "var H2=42; function f(){var i=0;load(H2, i)} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){var i=0;load(H2, i)} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "var f4=glob.SIMD.float32x4; function f(){var i=0;var vec=f4(1,2,3,4); store(H, i, vec)} return f");

//      Bad coercions of returned values
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){var i=0;return load(H, i)|0;} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){var i=0;return +load(H, i);} return f");

//      Literal index constants
var buf = new ArrayBuffer(BUF_MIN);
var asI32 = new Int32Array(buf);
asI32[(BUF_MIN >> 2) - 4] = 4;
asI32[(BUF_MIN >> 2) - 3] = 3;
asI32[(BUF_MIN >> 2) - 2] = 2;
asI32[(BUF_MIN >> 2) - 1] = 1;

assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load(H, " + (INT32_MAX + 1) + ");} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load(H, " + (INT32_MAX + 1 - 15) + ");} return f");
asmCompile('glob', 'ffi', 'heap', IMPORTS + "function f(){load(H, " + (INT32_MAX + 1 - 16) + ");} return f");

assertAsmLinkFail(asmCompile('glob', 'ffi', 'heap', IMPORTS + "function f() {return i4(load(H, " + (BUF_MIN - 15) + "));} return f"), this, {}, buf);
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', IMPORTS + "function f() {return i4(load(H, " + (BUF_MIN - 16) + "));} return f"), this, {}, buf)(), [4, 3, 2, 1]);
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', IMPORTS + "function f() {return i4(load(H, " + BUF_MIN + " - 16 | 0));} return f"), this, {}, buf)(), [4, 3, 2, 1]);

var CONSTANT_INDEX = 42;
var CONSTANT_BYTE_INDEX = CONSTANT_INDEX << 2;

var loadStoreCode = `
    "use asm";

    var H = new glob.Uint8Array(heap);

    var i4 = glob.SIMD.int32x4;
    var i4load = i4.load;
    var i4store = i4.store;

    var f4 = glob.SIMD.float32x4;
    var f4load = f4.load;
    var f4store = f4.store;

    function f32l(i) { i=i|0; return f4(f4load(H, i|0)); }
    function f32lcst() { return f4(f4load(H, ${CONSTANT_BYTE_INDEX})); }
    function f32s(i, vec) { i=i|0; vec=f4(vec); f4store(H, i|0, vec); }
    function f32scst(vec) { vec=f4(vec); f4store(H, ${CONSTANT_BYTE_INDEX}, vec); }

    function i32l(i) { i=i|0; return i4(i4load(H, i|0)); }
    function i32lcst() { return i4(i4load(H, ${CONSTANT_BYTE_INDEX})); }
    function i32s(i, vec) { i=i|0; vec=i4(vec); i4store(H, i|0, vec); }
    function i32scst(vec) { vec=i4(vec); i4store(H, ${CONSTANT_BYTE_INDEX}, vec); }

    function f32lbndcheck(i) {
        i=i|0;
        if ((i|0) > ${CONSTANT_BYTE_INDEX}) i=${CONSTANT_BYTE_INDEX};
        if ((i|0) < 0) i = 0;
        return f4(f4load(H, i|0));
    }
    function f32sbndcheck(i, vec) {
        i=i|0;
        vec=f4(vec);
        if ((i|0) > ${CONSTANT_BYTE_INDEX}) i=${CONSTANT_BYTE_INDEX};
        if ((i|0) < 0) i = 0;
        return f4(f4store(H, i|0, vec));
    }

    return {
        f32l: f32l,
        f32lcst: f32lcst,
        f32s: f32s,
        f32scst: f32scst,
        f32lbndcheck: f32lbndcheck,
        f32sbndcheck: f32sbndcheck,
        i32l: i32l,
        i32lcst: i32lcst,
        i32s: i32s,
        i32scst: i32scst
    }
`;

const SIZE = 0x8000;

var F32 = new Float32Array(SIZE);
var reset = function() {
    for (var i = 0; i < SIZE; i++)
        F32[i] = i + 1;
};
reset();

var buf = F32.buffer;
var m = asmLink(asmCompile('glob', 'ffi', 'heap', loadStoreCode), this, null, buf);

function slice(TA, i, n) { return Array.prototype.slice.call(TA, i, i + n); }

// Float32x4.load
function f32l(n) { return m.f32l((n|0) << 2 | 0); };

//      Correct accesses
assertEqX4(f32l(0), slice(F32, 0, 4));
assertEqX4(f32l(1), slice(F32, 1, 4));
assertEqX4(f32l(SIZE - 4), slice(F32, SIZE - 4, 4));

assertEqX4(m.f32lcst(), slice(F32, CONSTANT_INDEX, 4));
assertEqX4(m.f32lbndcheck(CONSTANT_BYTE_INDEX), slice(F32, CONSTANT_INDEX, 4));

//      OOB
assertThrowsInstanceOf(() => f32l(-1), RangeError);
assertThrowsInstanceOf(() => f32l(SIZE), RangeError);
assertThrowsInstanceOf(() => f32l(SIZE - 1), RangeError);
assertThrowsInstanceOf(() => f32l(SIZE - 2), RangeError);
assertThrowsInstanceOf(() => f32l(SIZE - 3), RangeError);

var code = `
    "use asm";
    var f4 = glob.SIMD.float32x4;
    var f4l = f4.load;
    var u8 = new glob.Uint8Array(heap);

    function g(x) {
        x = x|0;
        // set a constraint on the size of the heap
        var ptr = 0;
        ptr = u8[0xFFFF] | 0;
        // give a precise range to x
        x = (x>>0) > 5 ? 5 : x;
        x = (x>>0) < 0 ? 0 : x;
        // ptr value gets a precise range but the bounds check shouldn't get
        // eliminated.
        return f4(f4l(u8, 0xFFFA + x | 0));
    }

    return g;
`;
assertThrowsInstanceOf(() =>asmLink(asmCompile('glob', 'ffi', 'heap', code), this, {}, new ArrayBuffer(0x10000))(0), RangeError);

// Float32x4.store
function f32s(n, v) { return m.f32s((n|0) << 2 | 0, v); };

var vec  = SIMD.float32x4(5,6,7,8);
var vec2 = SIMD.float32x4(0,1,2,3);

reset();
f32s(0, vec);
assertEqX4(vec, slice(F32, 0, 4));

reset();
f32s(0, vec2);
assertEqX4(vec2, slice(F32, 0, 4));

reset();
f32s(4, vec);
assertEqX4(vec, slice(F32, 4, 4));

reset();
m.f32scst(vec2);
assertEqX4(vec2, slice(F32, CONSTANT_INDEX, 4));

reset();
m.f32sbndcheck(CONSTANT_BYTE_INDEX, vec);
assertEqX4(vec, slice(F32, CONSTANT_INDEX, 4));

//      OOB
reset();
assertThrowsInstanceOf(() => f32s(SIZE - 3, vec), RangeError);
assertThrowsInstanceOf(() => f32s(SIZE - 2, vec), RangeError);
assertThrowsInstanceOf(() => f32s(SIZE - 1, vec), RangeError);
assertThrowsInstanceOf(() => f32s(SIZE, vec), RangeError);
for (var i = 0; i < SIZE; i++)
    assertEq(F32[i], i + 1);

// Int32x4.load
var I32 = new Int32Array(buf);
reset = function () {
    for (var i = 0; i < SIZE; i++)
        I32[i] = i + 1;
};
reset();

function i32(n) { return m.i32l((n|0) << 2 | 0); };

//      Correct accesses
assertEqX4(i32(0), slice(I32, 0, 4));
assertEqX4(i32(1), slice(I32, 1, 4));
assertEqX4(i32(SIZE - 4), slice(I32, SIZE - 4, 4));

assertEqX4(m.i32lcst(), slice(I32, CONSTANT_INDEX, 4));

//      OOB
assertThrowsInstanceOf(() => i32(-1), RangeError);
assertThrowsInstanceOf(() => i32(SIZE), RangeError);
assertThrowsInstanceOf(() => i32(SIZE - 1), RangeError);
assertThrowsInstanceOf(() => i32(SIZE - 2), RangeError);
assertThrowsInstanceOf(() => i32(SIZE - 3), RangeError);

// Int32x4.store
function i32s(n, v) { return m.i32s((n|0) << 2 | 0, v); };

var vec  = SIMD.int32x4(5,6,7,8);
var vec2 = SIMD.int32x4(0,1,2,3);

reset();
i32s(0, vec);
assertEqX4(vec, slice(I32, 0, 4));

reset();
i32s(0, vec2);
assertEqX4(vec2, slice(I32, 0, 4));

reset();
i32s(4, vec);
assertEqX4(vec, slice(I32, 4, 4));

reset();
m.i32scst(vec2);
assertEqX4(vec2, slice(I32, CONSTANT_INDEX, 4));

//      OOB
reset();
assertThrowsInstanceOf(() => i32s(SIZE - 3, vec), RangeError);
assertThrowsInstanceOf(() => i32s(SIZE - 2, vec), RangeError);
assertThrowsInstanceOf(() => i32s(SIZE - 1, vec), RangeError);
assertThrowsInstanceOf(() => i32s(SIZE - 0, vec), RangeError);
for (var i = 0; i < SIZE; i++)
    assertEq(I32[i], i + 1);

