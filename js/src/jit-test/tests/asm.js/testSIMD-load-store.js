// |jit-test| test-also-noasmjs
load(libdir + "asm.js");
load(libdir + "simd.js");
load(libdir + "asserts.js");

// Set to true to see more JS debugging spew
const DEBUG = false;

if (!isSimdAvailable() || typeof SIMD === 'undefined') {
    DEBUG && print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

const INT32_MAX = Math.pow(2, 31) - 1;
const INT32_MIN = INT32_MAX + 1 | 0;

try {

// Load / Store
var IMPORTS = USE_ASM + 'var H=new glob.Uint8Array(heap); var i4=glob.SIMD.Int32x4; var ci4=i4.check; var load=i4.load; var store=i4.store;';

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
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "var f4=glob.SIMD.Float32x4; function f(){var i=0;var vec=f4(1,2,3,4); store(H, i, vec)} return f");

//      Bad coercions of returned values
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){var i=0;return load(H, i)|0;} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){var i=0;return +load(H, i);} return f");

//      Literal index constants
var buf = new ArrayBuffer(BUF_MIN);
var SIZE_TA = BUF_MIN >> 2
var asI32 = new Int32Array(buf);
asI32[SIZE_TA - 4] = 4;
asI32[SIZE_TA - 3] = 3;
asI32[SIZE_TA - 2] = 2;
asI32[SIZE_TA - 1] = 1;

assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load(H, -1);} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load(H, " + (INT32_MAX + 1) + ");} return f");
assertAsmTypeFail('glob', 'ffi', 'heap', IMPORTS + "function f(){load(H, " + (INT32_MAX + 1 - 15) + ");} return f");
asmCompile('glob', 'ffi', 'heap', IMPORTS + "function f(){load(H, " + (INT32_MAX + 1 - 16) + ");} return f");

assertAsmLinkFail(asmCompile('glob', 'ffi', 'heap', IMPORTS + "function f() {return ci4(load(H, " + (BUF_MIN - 15) + "));} return f"), this, {}, buf);
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', IMPORTS + "function f() {return ci4(load(H, " + (BUF_MIN - 16) + "));} return f"), this, {}, buf)(), [4, 3, 2, 1]);
assertEqX4(asmLink(asmCompile('glob', 'ffi', 'heap', IMPORTS + "function f() {return ci4(load(H, " + BUF_MIN + " - 16 | 0));} return f"), this, {}, buf)(), [4, 3, 2, 1]);

var CONSTANT_INDEX = 42;
var CONSTANT_BYTE_INDEX = CONSTANT_INDEX << 2;

var loadStoreCode = `
    "use asm";

    var H = new glob.Uint8Array(heap);

    var i4 = glob.SIMD.Int32x4;
    var i4load = i4.load;
    var i4store = i4.store;
    var ci4 = i4.check;

    var f4 = glob.SIMD.Float32x4;
    var f4load = f4.load;
    var f4store = f4.store;
    var cf4 = f4.check;

    function f32l(i) { i=i|0; return cf4(f4load(H, i|0)); }
    function f32lcst() { return cf4(f4load(H, ${CONSTANT_BYTE_INDEX})); }
    function f32s(i, vec) { i=i|0; vec=cf4(vec); f4store(H, i|0, vec); }
    function f32scst(vec) { vec=cf4(vec); f4store(H, ${CONSTANT_BYTE_INDEX}, vec); }

    function i32l(i) { i=i|0; return ci4(i4load(H, i|0)); }
    function i32lcst() { return ci4(i4load(H, ${CONSTANT_BYTE_INDEX})); }
    function i32s(i, vec) { i=i|0; vec=ci4(vec); i4store(H, i|0, vec); }
    function i32scst(vec) { vec=ci4(vec); i4store(H, ${CONSTANT_BYTE_INDEX}, vec); }

    function f32lbndcheck(i) {
        i=i|0;
        if ((i|0) > ${CONSTANT_BYTE_INDEX}) i=${CONSTANT_BYTE_INDEX};
        if ((i|0) < 0) i = 0;
        return cf4(f4load(H, i|0));
    }
    function f32sbndcheck(i, vec) {
        i=i|0;
        vec=cf4(vec);
        if ((i|0) > ${CONSTANT_BYTE_INDEX}) i=${CONSTANT_BYTE_INDEX};
        if ((i|0) < 0) i = 0;
        return cf4(f4store(H, i|0, vec));
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
    var f4 = glob.SIMD.Float32x4;
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
        return f4l(u8, 0xFFFA + x | 0);
    }

    return g;
`;
assertThrowsInstanceOf(() => asmLink(asmCompile('glob', 'ffi', 'heap', code), this, {}, new ArrayBuffer(0x10000))(0), RangeError);

// Float32x4.store
function f32s(n, v) { return m.f32s((n|0) << 2 | 0, v); };

var vec  = SIMD.Float32x4(5,6,7,8);
var vec2 = SIMD.Float32x4(0,1,2,3);
var vecWithNaN = SIMD.Float32x4(NaN, 2, NaN, 4);

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
f32s(4, vecWithNaN);
assertEqX4(vecWithNaN, slice(F32, 4, 4));

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

var vec  = SIMD.Int32x4(5,6,7,8);
var vec2 = SIMD.Int32x4(0,1,2,3);

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

// Partial loads and stores
(function() {

//      Variable indexes
function MakeCodeFor(typeName) {
    return `
    "use asm";
    var type = glob.SIMD.${typeName};
    var c = type.check;

    var l1 = type.load1;
    var l2 = type.load2;
    var l3 = type.load3;

    var s1 = type.store1;
    var s2 = type.store2;
    var s3 = type.store3;

    var u8 = new glob.Uint8Array(heap);

    function load1(i) { i=i|0; return l1(u8, i); }
    function load2(i) { i=i|0; return l2(u8, i); }
    function load3(i) { i=i|0; return l3(u8, i); }

    function loadCst1() { return l1(u8, 41 << 2); }
    function loadCst2() { return l2(u8, 41 << 2); }
    function loadCst3() { return l3(u8, 41 << 2); }

    function store1(i, x) { i=i|0; x=c(x); return s1(u8, i, x); }
    function store2(i, x) { i=i|0; x=c(x); return s2(u8, i, x); }
    function store3(i, x) { i=i|0; x=c(x); return s3(u8, i, x); }

    function storeCst1(x) { x=c(x); return s1(u8, 41 << 2, x); }
    function storeCst2(x) { x=c(x); return s2(u8, 41 << 2, x); }
    function storeCst3(x) { x=c(x); return s3(u8, 41 << 2, x); }

    return {
        load1: load1,
        load2: load2,
        load3: load3,
        loadCst1: loadCst1,
        loadCst2: loadCst2,
        loadCst3: loadCst3,
        store1: store1,
        store2: store2,
        store3: store3,
        storeCst1: storeCst1,
        storeCst2: storeCst2,
        storeCst3: storeCst3,
    }
`;
}

var SIZE = 0x10000;

function TestPartialLoads(m, typedArray, x, y, z, w) {
    // Fill array with predictable values
    for (var i = 0; i < SIZE; i += 4) {
        typedArray[i] =     x(i);
        typedArray[i + 1] = y(i);
        typedArray[i + 2] = z(i);
        typedArray[i + 3] = w(i);
    }

    // Test correct loads
    var i = 0, j = 0; // i in elems, j in bytes
    assertEqX4(m.load1(j), [x(i), 0, 0, 0]);
    assertEqX4(m.load2(j), [x(i), y(i), 0, 0]);
    assertEqX4(m.load3(j), [x(i), y(i), z(i), 0]);

    j += 4;
    assertEqX4(m.load1(j), [y(i), 0, 0, 0]);
    assertEqX4(m.load2(j), [y(i), z(i), 0, 0]);
    assertEqX4(m.load3(j), [y(i), z(i), w(i), 0]);

    j += 4;
    assertEqX4(m.load1(j), [z(i), 0, 0, 0]);
    assertEqX4(m.load2(j), [z(i), w(i), 0, 0]);
    assertEqX4(m.load3(j), [z(i), w(i), x(i+4), 0]);

    j += 4;
    assertEqX4(m.load1(j), [w(i), 0, 0, 0]);
    assertEqX4(m.load2(j), [w(i), x(i+4), 0, 0]);
    assertEqX4(m.load3(j), [w(i), x(i+4), y(i+4), 0]);

    j += 4;
    i += 4;
    assertEqX4(m.load1(j), [x(i), 0, 0, 0]);
    assertEqX4(m.load2(j), [x(i), y(i), 0, 0]);
    assertEqX4(m.load3(j), [x(i), y(i), z(i), 0]);

    // Test loads with constant indexes (41)
    assertEqX4(m.loadCst1(), [y(40), 0, 0, 0]);
    assertEqX4(m.loadCst2(), [y(40), z(40), 0, 0]);
    assertEqX4(m.loadCst3(), [y(40), z(40), w(40), 0]);

    // Test limit and OOB accesses
    assertEqX4(m.load1((SIZE - 1) << 2), [w(SIZE - 4), 0, 0, 0]);
    assertThrowsInstanceOf(() => m.load1(((SIZE - 1) << 2) + 1), RangeError);

    assertEqX4(m.load2((SIZE - 2) << 2), [z(SIZE - 4), w(SIZE - 4), 0, 0]);
    assertThrowsInstanceOf(() => m.load2(((SIZE - 2) << 2) + 1), RangeError);

    assertEqX4(m.load3((SIZE - 3) << 2), [y(SIZE - 4), z(SIZE - 4), w(SIZE - 4), 0]);
    assertThrowsInstanceOf(() => m.load3(((SIZE - 3) << 2) + 1), RangeError);
}

// Partial stores
function TestPartialStores(m, typedArray, typeName, x, y, z, w) {
    var val = SIMD[typeName](x, y, z, w);

    function Reset() {
        for (var i = 0; i < SIZE; i++)
            typedArray[i] = i + 1;
    }
    function CheckNotModified(low, high) {
        for (var i = low; i < high; i++)
            assertEq(typedArray[i], i + 1);
    }

    function TestStore1(i) {
        m.store1(i, val);
        CheckNotModified(0, i >> 2);
        assertEq(typedArray[i >> 2], x);
        CheckNotModified((i >> 2) + 1, SIZE);
        typedArray[i >> 2] = (i >> 2) + 1;
    }

    function TestStore2(i) {
        m.store2(i, val);
        CheckNotModified(0, i >> 2);
        assertEq(typedArray[i >> 2], x);
        assertEq(typedArray[(i >> 2) + 1], y);
        CheckNotModified((i >> 2) + 2, SIZE);
        typedArray[i >> 2] = (i >> 2) + 1;
        typedArray[(i >> 2) + 1] = (i >> 2) + 2;
    }

    function TestStore3(i) {
        m.store3(i, val);
        CheckNotModified(0, i >> 2);
        assertEq(typedArray[i >> 2], x);
        assertEq(typedArray[(i >> 2) + 1], y);
        assertEq(typedArray[(i >> 2) + 2], z);
        CheckNotModified((i >> 2) + 3, SIZE);
        typedArray[i >> 2] = (i >> 2) + 1;
        typedArray[(i >> 2) + 1] = (i >> 2) + 2;
        typedArray[(i >> 2) + 2] = (i >> 2) + 3;
    }

    function TestOOBStore(f) {
        assertThrowsInstanceOf(f, RangeError);
        CheckNotModified(0, SIZE);
    }

    Reset();

    TestStore1(0);
    TestStore1(1 << 2);
    TestStore1(2 << 2);
    TestStore1(3 << 2);
    TestStore1(1337 << 2);

    var i = (SIZE - 1) << 2;
    TestStore1(i);
    TestOOBStore(() => m.store1(i + 1, val));
    TestOOBStore(() => m.store1(-1, val));

    TestStore2(0);
    TestStore2(1 << 2);
    TestStore2(2 << 2);
    TestStore2(3 << 2);
    TestStore2(1337 << 2);

    var i = (SIZE - 2) << 2;
    TestStore2(i);
    TestOOBStore(() => m.store2(i + 1, val));
    TestOOBStore(() => m.store2(-1, val));

    TestStore3(0);
    TestStore3(1 << 2);
    TestStore3(2 << 2);
    TestStore3(3 << 2);
    TestStore3(1337 << 2);

    var i = (SIZE - 3) << 2;
    TestStore3(i);
    TestOOBStore(() => m.store3(i + 1, val));
    TestOOBStore(() => m.store3(-1, val));
    TestOOBStore(() => m.store3(-9, val));

    // Constant indexes (41)
    m.storeCst1(val);
    CheckNotModified(0, 41);
    assertEq(typedArray[41], x);
    CheckNotModified(42, SIZE);
    typedArray[41] = 42;

    m.storeCst2(val);
    CheckNotModified(0, 41);
    assertEq(typedArray[41], x);
    assertEq(typedArray[42], y);
    CheckNotModified(43, SIZE);
    typedArray[41] = 42;
    typedArray[42] = 43;

    m.storeCst3(val);
    CheckNotModified(0, 41);
    assertEq(typedArray[41], x);
    assertEq(typedArray[42], y);
    assertEq(typedArray[43], z);
    CheckNotModified(44, SIZE);
    typedArray[41] = 42;
    typedArray[42] = 43;
    typedArray[43] = 44;
}

var f32 = new Float32Array(SIZE);
var mFloat32x4 = asmLink(asmCompile('glob', 'ffi', 'heap', MakeCodeFor('Float32x4')), this, null, f32.buffer);

TestPartialLoads(mFloat32x4, f32,
            (i) => i + 1,
            (i) => Math.fround(13.37),
            (i) => Math.fround(1/i),
            (i) => Math.fround(Math.sqrt(0x2000 - i)));

TestPartialStores(mFloat32x4, f32, 'Float32x4', 42, -0, NaN, 0.1337);

var i32 = new Int32Array(f32.buffer);
var mInt32x4 = asmLink(asmCompile('glob', 'ffi', 'heap', MakeCodeFor('Int32x4')), this, null, i32.buffer);

TestPartialLoads(mInt32x4, i32,
            (i) => i + 1 | 0,
            (i) => -i | 0,
            (i) => i * 2 | 0,
            (i) => 42);

TestPartialStores(mInt32x4, i32, 'Int32x4', 42, -3, 13, 37);

})();

} catch (e) { print('stack: ', e.stack); throw e }
