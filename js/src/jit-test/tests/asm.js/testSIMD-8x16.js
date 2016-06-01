load(libdir + "asm.js");
load(libdir + "simd.js");
load(libdir + "asserts.js");

// Set to true to see more JS debugging spew.
const DEBUG = false;

if (!isSimdAvailable()) {
    DEBUG && print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

// Tests for 8x16 SIMD types: Int8x16, Uint8x16, Bool8x16.

const I8x16 = 'var i8x16 = glob.SIMD.Int8x16;'
const I8x16CHK = 'var i8x16chk = i8x16.check;'
const I8x16EXT = 'var i8x16ext = i8x16.extractLane;'
const I8x16REP = 'var i8x16rep = i8x16.replaceLane;'
const I8x16U8x16 = 'var i8x16u8x16 = i8x16.fromUint8x16Bits;'

const U8x16 = 'var u8x16 = glob.SIMD.Uint8x16;'
const U8x16CHK = 'var u8x16chk = u8x16.check;'
const U8x16EXT = 'var u8x16ext = u8x16.extractLane;'
const U8x16REP = 'var u8x16rep = u8x16.replaceLane;'
const U8x16I8x16 = 'var u8x16i8x16 = u8x16.fromInt8x16Bits;'

const B8x16 = 'var b8x16 = glob.SIMD.Bool8x16;'
const B8x16CHK = 'var b8x16chk = b8x16.check;'
const B8x16EXT = 'var b8x16ext = b8x16.extractLane;'
const B8x16REP = 'var b8x16rep = b8x16.replaceLane;'

const INT8_MAX = 127
const INT8_MIN = -128
const UINT8_MAX = 255

// Linking
assertEq(asmLink(asmCompile('glob', USE_ASM + I8x16 + "function f() {} return f"), {SIMD:{Int8x16: SIMD.Int8x16}})(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + U8x16 + "function f() {} return f"), {SIMD:{Uint8x16: SIMD.Uint8x16}})(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + B8x16 + "function f() {} return f"), {SIMD:{Bool8x16: SIMD.Bool8x16}})(), undefined);

// Local variable of Int8x16 type.
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=Int8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);} return f");
assertAsmTypeFail('glob', USE_ASM + I8x16 + "function f() {var x=i8x16;} return f");
assertAsmTypeFail('glob', USE_ASM + I8x16 + "function f() {var x=i8x16();} return f");
assertAsmTypeFail('glob', USE_ASM + I8x16 + "function f() {var x=i8x16(1);} return f");
assertAsmTypeFail('glob', USE_ASM + I8x16 + "function f() {var x=i8x16(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + I8x16 + "function f() {var x=i8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16.0);} return f");
assertAsmTypeFail('glob', USE_ASM + I8x16 + "function f() {var x=i8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17);} return f");
assertAsmTypeFail('glob', USE_ASM + I8x16 + "function f() {var x=i8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16|0);} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + I8x16 + "function f() {var x=i8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + I8x16 + "function f() {var x=i8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15," + (INT8_MAX + 1) + ");} return f"), this)(), undefined);

// Local variable of Uint8x16 type.
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=Uint8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);} return f");
assertAsmTypeFail('glob', USE_ASM + U8x16 + "function f() {var x=u8x16;} return f");
assertAsmTypeFail('glob', USE_ASM + U8x16 + "function f() {var x=u8x16();} return f");
assertAsmTypeFail('glob', USE_ASM + U8x16 + "function f() {var x=u8x16(1);} return f");
assertAsmTypeFail('glob', USE_ASM + U8x16 + "function f() {var x=u8x16(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + U8x16 + "function f() {var x=u8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16.0);} return f");
assertAsmTypeFail('glob', USE_ASM + U8x16 + "function f() {var x=u8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17);} return f");
assertAsmTypeFail('glob', USE_ASM + U8x16 + "function f() {var x=u8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16|0);} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + U8x16 + "function f() {var x=u8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + U8x16 + "function f() {var x=u8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15," + (UINT8_MAX + 1) + ");} return f"), this)(), undefined);

// Local variable of Bool8x16 type.
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=Bool8x16(1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1);} return f");
assertAsmTypeFail('glob', USE_ASM + B8x16 + "function f() {var x=b8x16;} return f");
assertAsmTypeFail('glob', USE_ASM + B8x16 + "function f() {var x=b8x16();} return f");
assertAsmTypeFail('glob', USE_ASM + B8x16 + "function f() {var x=b8x16(1);} return f");
assertAsmTypeFail('glob', USE_ASM + B8x16 + "function f() {var x=b8x16(1,0,0,0);} return f");
assertAsmTypeFail('glob', USE_ASM + B8x16 + "function f() {var x=b8x16(1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1.0);} return f");
assertAsmTypeFail('glob', USE_ASM + B8x16 + "function f() {var x=b8x16(1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1|0);} return f");
assertAsmTypeFail('glob', USE_ASM + B8x16 + "function f() {var x=b8x16(1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1);} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + B8x16 + "function f() {var x=b8x16(1,0,0,0,0,0,0,0,0,1,-1,2,-2,1,1,1);} return f"), this)(), undefined);

// Only signed Int8x16 allowed as return value.
assertEqVecArr(asmLink(asmCompile('glob', USE_ASM + I8x16 + "function f() {return i8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);} return f"), this)(),
           [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
assertEqVecArr(asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK + "function f() {return i8x16chk(i8x16(1,2,3,132,5,6,7,8,9,10,11,12,13,14,15,16));} return f"), this)(),
           [1, 2, 3, -124, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
assertAsmTypeFail('glob', USE_ASM + U8x16 + "function f() {return u8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);} return f");
assertAsmTypeFail('glob', USE_ASM + U8x16 + U8x16CHK + "function f() {return u8x16chk(u8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16));} return f");

// Test splat.
function splat(x) {
    let r = []
    for (let i = 0; i < 16; i++)
        r.push(x);
    return r
}

splatB = asmLink(asmCompile('glob', USE_ASM + B8x16 +
                            'var splat = b8x16.splat;' +
                            'function f(x) { x = x|0; return splat(x); } return f'), this);
assertEqVecArr(splatB(true), splat(true));
assertEqVecArr(splatB(false), splat(false));


splatB0 = asmLink(asmCompile('glob', USE_ASM + B8x16 +
                             'var splat = b8x16.splat;' +
                             'function f() { var x = 0; return splat(x); } return f'), this);
assertEqVecArr(splatB0(), splat(false));
splatB1 = asmLink(asmCompile('glob', USE_ASM + B8x16 +
                             'var splat = b8x16.splat;' +
                             'function f() { var x = 1; return splat(x); } return f'), this);
assertEqVecArr(splatB1(), splat(true));

splatI = asmLink(asmCompile('glob', USE_ASM + I8x16 +
                            'var splat = i8x16.splat;' +
                            'function f(x) { x = x|0; return splat(x); } return f'), this);
for (let x of [0, 1, -1, 0x1234, 0x12, 1000, -1000000]) {
    assertEqVecArr(splatI(x), splat(x << 24 >> 24));
}

splatIc = asmLink(asmCompile('glob', USE_ASM + I8x16 +
                             'var splat = i8x16.splat;' +
                             'function f() { var x = 100; return splat(x); } return f'), this);
assertEqVecArr(splatIc(), splat(100))

splatU = asmLink(asmCompile('glob', USE_ASM + U8x16 + I8x16 + I8x16U8x16 +
                            'var splat = u8x16.splat;' +
                            'function f(x) { x = x|0; return i8x16u8x16(splat(x)); } return f'), this);
for (let x of [0, 1, -1, 0x1234, 0x12, 1000, -1000000]) {
    assertEqVecArr(SIMD.Uint8x16.fromInt8x16Bits(splatI(x)), splat(x << 24 >>> 24));
}

splatUc = asmLink(asmCompile('glob', USE_ASM + U8x16 + I8x16 + I8x16U8x16 +
                            'var splat = u8x16.splat;' +
                            'function f() { var x = 200; return i8x16u8x16(splat(x)); } return f'), this);
assertEqVecArr(SIMD.Uint8x16.fromInt8x16Bits(splatUc()), splat(200))


// Test extractLane.
//
// The lane index must be a literal int, and we generate different code for
// different lanes.
function extractI(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16EXT +
                            `function f() {var x=i8x16(${a.join(',')}); return i8x16ext(x, ${i})|0; } return f`), this)();
}
a = [-1,2,-3,4,-5,6,-7,8,-9,10,-11,12,-13,-14,-15,-16];
for (var i = 0; i < 16; i++)
  assertEq(extractI(a, i), a[i]);
a = a.map(x => -x);
for (var i = 0; i < 16; i++)
  assertEq(extractI(a, i), a[i]);

function extractU(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + U8x16 + U8x16EXT +
                            `function f() {var x=u8x16(${a.join(',')}); return u8x16ext(x, ${i})|0; } return f`), this)();
}
a = [1,255,12,13,14,150,200,3,4,5,6,7,8,9,10,16];
for (var i = 0; i < 16; i++)
  assertEq(extractU(a, i), a[i]);
a = a.map(x => 255-x);
for (var i = 0; i < 16; i++)
  assertEq(extractU(a, i), a[i]);

function extractB(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + B8x16 + B8x16EXT +
                            `function f() {var x=b8x16(${a.join(',')}); return b8x16ext(x, ${i})|0; } return f`), this)();
}
a = [1,1,0,1,1,0,0,0,1,1,1,1,0,0,0,1];
for (var i = 0; i < 16; i++)
  assertEq(extractB(a, i), a[i]);
a = a.map(x => 1-x);
for (var i = 0; i < 16; i++)
  assertEq(extractB(a, i), a[i]);

// Test replaceLane.
function replaceI(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16REP +
                            `function f(v) {v=v|0; var x=i8x16(${a.join(',')}); return i8x16rep(x,${i},v); } return f`), this);
}
a = [-1,2,-3,4,-5,6,-7,8,-9,10,-11,12,-13,-14,-15,-16];
for (var i = 0; i < 16; i++) {
    var f = replaceI(a, i);
    var b = a.slice(0);
    b[i] = -20;
    assertEqVecArr(f(-20), b);
}

function replaceU(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + U8x16 + U8x16REP + I8x16 + I8x16U8x16 +
                            `function f(v) {v=v|0; var x=u8x16(${a.join(',')}); x=u8x16rep(x,${i},v); return i8x16u8x16(x); } return f`), this);
}
a = [256-1,2,256-3,4,256-5,6,256-7,8,256-9,10,256-11,12,256-13,256-14,256-15,256-16];
for (var i = 0; i < 16; i++) {
    // Result returned as Int8x16, convert back.
    var rawf = replaceU(a, i);
    var f = x => SIMD.Uint8x16.fromInt8x16Bits(rawf(x));
    var b = a.slice(0);
    b[i] = 100;
    assertEqVecArr(f(100), b);
}

function replaceB(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + B8x16 + B8x16REP +
                            `function f(v) {v=v|0; var x=b8x16(${a.join(',')}); return b8x16rep(x,${i},v); } return f`), this);
}
a = [1,1,0,1,1,0,0,0,1,1,1,1,0,0,0,1];
for (var i = 0; i < 16; i++) {
    var f = replaceB(a, i);
    var b = a.slice(0);
    v = 1 - a[i];
    b[i] = v;
    assertEqVecArr(f(v), b.map(x => !!x));
}


// Test select.
selectI = asmLink(asmCompile('glob', USE_ASM + I8x16 + B8x16 + B8x16CHK +
                             'var select = i8x16.select;' +
                             'var a = i8x16(-1,2,-3,4,-5, 6,-7, 8,-9,10,-11,12,-13,-14,-15,-16);' +
                             'var b = i8x16( 5,6, 7,8, 9,10,11,12,13,14, 15,16,-77, 45, 32,  0);' +
                             'function f(x) { x = b8x16chk(x); return select(x, a, b); } return f'), this);
assertEqVecArr(selectI(SIMD.Bool8x16( 0,0, 1,0, 1,1, 1, 0, 1, 1, 0, 0,  1,  1, 0,  1)),
                                    [ 5,6,-3,8,-5,6,-7,12,-9,10,15,16,-13,-14,32,-16]);

selectU = asmLink(asmCompile('glob', USE_ASM + I8x16 + B8x16 + B8x16CHK + U8x16 + I8x16U8x16 + U8x16I8x16 +
                             'var select = u8x16.select;' +
                             'var a = i8x16(-1,2,-3,4,-5, 6,-7, 8,-9,10,-11,12,-13,-14,-15,-16);' +
                             'var b = i8x16( 5,6, 7,8, 9,10,11,12,13,14, 15,16,-77, 45, 32,  0);' +
                             'function f(x) { x = b8x16chk(x); return i8x16u8x16(select(x, u8x16i8x16(a), u8x16i8x16(b))); } return f'), this);
assertEqVecArr(selectU(SIMD.Bool8x16( 0,0, 1,0, 1,1, 1, 0, 1, 1, 0, 0,  1,  1, 0,  1)),
                                    [ 5,6,-3,8,-5,6,-7,12,-9,10,15,16,-13,-14,32,-16]);


// Test swizzle.
function swizzle(vec, lanes) {
    let r = [];
    for (let i = 0; i < 16; i++)
        r.push(vec[lanes[i]]);
    return r;
}

function swizzleI(lanes) {
    let asm = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK +
                                 'var swz = i8x16.swizzle;' +
                                 `function f(a) { a = i8x16chk(a); return swz(a, ${lanes.join()}); } return f`), this);
    let a1 = [  -1,2,  -3,-128,0x7f,6,-7, 8,-9, 10,-11, 12,-13,-14,-15, -16];
    let a2 = [-128,2,-128,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,  -1];
    let v1 = SIMD.Int8x16(...a1);
    let v2 = SIMD.Int8x16(...a2);
    assertEqVecArr(asm(v1), swizzle(a1, lanes));
    assertEqVecArr(asm(v2), swizzle(a2, lanes));
}

swizzleI([10, 1, 7, 5, 1, 2, 6, 8, 5, 13, 0, 6, 2, 8, 0, 9]);
swizzleI([ 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0]);
swizzleI([15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15]);

function swizzleU(lanes) {
    let asm = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK + U8x16 + U8x16I8x16 + I8x16U8x16 +
                                 'var swz = u8x16.swizzle;' +
                                 `function f(a) { a = i8x16chk(a); return i8x16u8x16(swz(u8x16i8x16(a), ${lanes.join()})); } return f`), this);
    let a1 = [  -1,2,  -3,-128,0x7f,6,-7, 8,-9, 10,-11, 12,-13,-14,-15, -16];
    let a2 = [-128,2,-128,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,  -1];
    let v1 = SIMD.Int8x16(...a1);
    let v2 = SIMD.Int8x16(...a2);
    assertEqVecArr(asm(v1), swizzle(a1, lanes));
    assertEqVecArr(asm(v2), swizzle(a2, lanes));
}

swizzleU([10, 1, 7, 5, 1, 2, 6, 8, 5, 13, 0, 6, 2, 8, 0, 9]);
swizzleU([ 0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0]);
swizzleU([15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15]);

// Out-of-range lane indexes.
assertAsmTypeFail('glob', USE_ASM + I8x16 + 'var swz = i8x16.swizzle; ' +
                  'function f() { var x=i8x16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0); swz(x,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16); } return f');
assertAsmTypeFail('glob', USE_ASM + U8x16 + 'var swz = u8x16.swizzle; ' +
                  'function f() { var x=u8x16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0); swz(x,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16); } return f');
// Missing lane indexes.
assertAsmTypeFail('glob', USE_ASM + I8x16 + 'var swz = i8x16.swizzle; ' +
                  'function f() { var x=i8x16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0); swz(x,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15); } return f');
assertAsmTypeFail('glob', USE_ASM + U8x16 + 'var swz = u8x16.swizzle; ' +
                  'function f() { var x=u8x16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0); swz(x,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15); } return f');


// Test shuffle.
function shuffle(vec1, vec2, lanes) {
    let r = [];
    let vec = vec1.concat(vec2);
    for (let i = 0; i < 16; i++)
        r.push(vec[lanes[i]]);
    return r;
}

function shuffleI(lanes) {
    let asm = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK +
                                 'var shuf = i8x16.shuffle;' +
                                 `function f(a1, a2) { a1 = i8x16chk(a1); a2 = i8x16chk(a2); return shuf(a1, a2, ${lanes.join()}); } return f`), this);
    let a1 = [  -1,2,  -3,-128,0x7f,6,-7, 8,-9, 10,-11, 12,-13,-14,-15, -16];
    let a2 = [-128,2,-128,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,  -1];
    let v1 = SIMD.Int8x16(...a1);
    let v2 = SIMD.Int8x16(...a2);
    assertEqVecArr(asm(v1, v2), shuffle(a1, a2, lanes));
}

shuffleI([31, 9, 5, 4, 29, 12, 19, 10, 16, 22, 10, 9, 6, 18, 9, 8]);
shuffleI([ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
shuffleI([31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31]);

function shuffleU(lanes) {
    let asm = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK + U8x16 + U8x16I8x16 + I8x16U8x16 +
                                 'var shuf = u8x16.shuffle;' +
                                 'function f(a1, a2) { a1 = i8x16chk(a1); a2 = i8x16chk(a2); ' +
                                 `return i8x16u8x16(shuf(u8x16i8x16(a1), u8x16i8x16(a2), ${lanes.join()})); } return f`), this);
    let a1 = [  -1,2,  -3,-128,0x7f,6,-7, 8,-9, 10,-11, 12,-13,-14,-15, -16];
    let a2 = [-128,2,-128,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,  -1];
    let v1 = SIMD.Int8x16(...a1);
    let v2 = SIMD.Int8x16(...a2);
    assertEqVecArr(asm(v1, v2), shuffle(a1, a2, lanes));
}

shuffleU([31, 9, 5, 4, 29, 12, 19, 10, 16, 22, 10, 9, 6, 18, 9, 8]);
shuffleU([ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
shuffleU([31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31]);


// Out-of-range lane indexes.
assertAsmTypeFail('glob', USE_ASM + I8x16 + 'var shuf = i8x16.shuffle; ' +
                  'function f() { var x=i8x16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0); shuf(x,x,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,32); } return f');
assertAsmTypeFail('glob', USE_ASM + U8x16 + 'var shuf = u8x16.shuffle; ' +
                  'function f() { var x=u8x16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0); shuf(x,x,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,32); } return f');
// Missing lane indexes.
assertAsmTypeFail('glob', USE_ASM + I8x16 + 'var shuf = i8x16.shuffle; ' +
                  'function f() { var x=i8x16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0); shuf(x,x,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15); } return f');
assertAsmTypeFail('glob', USE_ASM + U8x16 + 'var shuf = u8x16.shuffle; ' +
                  'function f() { var x=u8x16(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0); shuf(x,x,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15); } return f');


// Test unary operators.
function unaryI(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK +
                                      `var fut = i8x16.${opname};` +
                                      'function f(v) { v = i8x16chk(v); return fut(v); } return f'), this);
    let a = [-1,2,-3,4,-5,6,-7,8,-9,10,-11,12,-13,-14,-15,-16];
    let v = SIMD.Int8x16(...a);
    assertEqVecArr(simdfunc(v), a.map(lanefunc));
}

function unaryU(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + U8x16 + I8x16 + I8x16CHK + U8x16I8x16 + I8x16U8x16 +
                                      `var fut = u8x16.${opname};` +
                                      'function f(v) { v = i8x16chk(v); return i8x16u8x16(fut(u8x16i8x16(v))); } return f'), this);
    let a = [256-1,2,256-3,4,256-5,6,256-7,8,256-9,10,256-11,12,256-13,256-14,256-15,256-16];
    let v = SIMD.Int8x16(...a);
    assertEqVecArr(SIMD.Uint8x16.fromInt8x16Bits(simdfunc(v)), a.map(lanefunc));
}

function unaryB(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + B8x16 + B8x16CHK +
                                      `var fut = b8x16.${opname};` +
                                      'function f(v) { v = b8x16chk(v); return fut(v); } return f'), this);
    let a = [1,1,0,1,1,0,0,0,1,1,1,1,0,0,0,1];
    let v = SIMD.Bool8x16(...a);
    assertEqVecArr(simdfunc(v), a.map(lanefunc));
}

unaryI('not', x => ~x << 24 >> 24);
unaryU('not', x => ~x << 24 >>> 24);
unaryB('not', x => !x);
unaryI('neg', x => -x << 24 >> 24);
unaryU('neg', x => -x << 24 >>> 24);


// Test binary operators.
function zipmap(a1, a2, f) {
    assertEq(a1.length, a2.length);
    let r = [];
    for (var i = 0; i < a1.length; i++)
        r.push(f(a1[i], a2[i]));
    return r
}

function binaryI(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK +
                                      `var fut = i8x16.${opname};` +
                                      'function f(v1, v2) { v1 = i8x16chk(v1); v2 = i8x16chk(v2); return fut(v1, v2); } return f'), this);
    let a1 = [  -1,2,  -3,-128,0x7f,6,-7, 8,-9, 10,-11, 12,-13,-14,-15, -16];
    let a2 = [-128,2,-128,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,  -1];
    let ref = zipmap(a1, a2, lanefunc);
    let v1 = SIMD.Int8x16(...a1);
    let v2 = SIMD.Int8x16(...a2);
    assertEqVecArr(simdfunc(v1, v2), ref);
}

function binaryU(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + U8x16 + I8x16 + I8x16CHK + U8x16I8x16 + I8x16U8x16 +
                                      `var fut = u8x16.${opname};` +
                                      'function f(v1, v2) { v1 = i8x16chk(v1); v2 = i8x16chk(v2); return i8x16u8x16(fut(u8x16i8x16(v1), u8x16i8x16(v2))); } return f'), this);
    let a1 = [  -1,2,  -3,0x80,0x7f,6,-7, 8,-9, 10,-11, 12,-13,-14,-15, -16].map(x => x & 0xff);
    let a2 = [0x80,2,0x80,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,0xff].map(x => x & 0xff);
    let ref = zipmap(a1, a2, lanefunc);
    let v1 = SIMD.Int8x16(...a1);
    let v2 = SIMD.Int8x16(...a2);
    let res = SIMD.Uint8x16.fromInt8x16Bits(simdfunc(v1, v2));
    assertEqVecArr(res, ref);
}

function binaryB(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + B8x16 + B8x16CHK +
                                      `var fut = b8x16.${opname};` +
                                      'function f(v1, v2) { v1 = b8x16chk(v1); v2 = b8x16chk(v2); return fut(v1, v2); } return f'), this);
    let a = [1,1,0,1,1,0,0,0,1,1,1,1,0,0,0,1];
    let v = SIMD.Bool8x16(...a);
    assertEqVecArr(simdfunc(v), a.map(lanefunc));
}

binaryI('add', (x, y) => (x + y) << 24 >> 24);
binaryI('sub', (x, y) => (x - y) << 24 >> 24);
binaryI('mul', (x, y) => (x * y) << 24 >> 24);
binaryU('add', (x, y) => (x + y) << 24 >>> 24);
binaryU('sub', (x, y) => (x - y) << 24 >>> 24);
binaryU('mul', (x, y) => (x * y) << 24 >>> 24);

binaryI('and', (x, y) => (x & y) << 24 >> 24);
binaryI('or',  (x, y) => (x | y) << 24 >> 24);
binaryI('xor', (x, y) => (x ^ y) << 24 >> 24);
binaryU('and', (x, y) => (x & y) << 24 >>> 24);
binaryU('or',  (x, y) => (x | y) << 24 >>> 24);
binaryU('xor', (x, y) => (x ^ y) << 24 >>> 24);

function sat(x, lo, hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x
}
function isat(x) { return sat(x, -128, 127); }
function usat(x) { return sat(x, 0, 255); }

binaryI('addSaturate', (x, y) => isat(x + y))
binaryI('subSaturate', (x, y) => isat(x - y))
binaryU('addSaturate', (x, y) => usat(x + y))
binaryU('subSaturate', (x, y) => usat(x - y))

// Test shift operators.
function zip1map(a, s, f) {
    return a.map(x => f(x, s));
}

function shiftI(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK +
                                      `var fut = i8x16.${opname};` +
                                      'function f(v, s) { v = i8x16chk(v); s = s|0; return fut(v, s); } return f'), this);
    let a = [0x80,2,0x80,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,0xff];
    let v = SIMD.Int8x16(...a);
    for (let s of [0, 1, 2, 6, 7, 8, 9, 10, 16, 255, -1, -8, -7, -1000]) {
        let ref = zip1map(a, s, lanefunc);
        // 1. Test dynamic shift amount.
        assertEqVecArr(simdfunc(v, s), ref);

        // 2. Test constant shift amount.
        let cstf = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK +
                                      `var fut = i8x16.${opname};` +
                                      `function f(v) { v = i8x16chk(v); return fut(v, ${s}); } return f`), this);
        assertEqVecArr(cstf(v, s), ref);
    }
}

function shiftU(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + U8x16 + I8x16 + I8x16CHK + U8x16I8x16 + I8x16U8x16 +
                                      `var fut = u8x16.${opname};` +
                                      'function f(v, s) { v = i8x16chk(v); s = s|0; return i8x16u8x16(fut(u8x16i8x16(v), s)); } return f'), this);
    let a = [0x80,2,0x80,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,0xff];
    let v = SIMD.Int8x16(...a);
    for (let s of [0, 1, 2, 6, 7, 8, 9, 10, 16, 255, -1, -8, -7, -1000]) {
        let ref = zip1map(a, s, lanefunc);
        // 1. Test dynamic shift amount.
        assertEqVecArr(SIMD.Uint8x16.fromInt8x16Bits(simdfunc(v, s)), ref);

        // 2. Test constant shift amount.
        let cstf = asmLink(asmCompile('glob', USE_ASM + U8x16 + I8x16 + I8x16CHK + U8x16I8x16 + I8x16U8x16 +
                                      `var fut = u8x16.${opname};` +
                                      `function f(v) { v = i8x16chk(v); return i8x16u8x16(fut(u8x16i8x16(v), ${s})); } return f`), this);
        assertEqVecArr(SIMD.Uint8x16.fromInt8x16Bits(cstf(v, s)), ref);
    }
}

shiftI('shiftLeftByScalar', (x,s) => (x << (s & 7)) << 24 >> 24);
shiftU('shiftLeftByScalar', (x,s) => (x << (s & 7)) << 24 >>> 24);
shiftI('shiftRightByScalar', (x,s) => ((x << 24 >> 24) >> (s & 7)) << 24 >> 24);
shiftU('shiftRightByScalar', (x,s) => ((x << 24 >>> 24) >>> (s & 7)) << 24 >>> 24);


// Comparisons.
function compareI(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK +
                                      `var fut = i8x16.${opname};` +
                                      'function f(v1, v2) { v1 = i8x16chk(v1); v2 = i8x16chk(v2); return fut(v1, v2); } return f'), this);
    let a1 = [  -1,2,  -3,-128,0x7f,6,-7, 8,-9, 10,-11, 12,-13,-14,-15, -16];
    let a2 = [-128,2,-128,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,  -1];
    let ref = zipmap(a1, a2, lanefunc);
    let v1 = SIMD.Int8x16(...a1);
    let v2 = SIMD.Int8x16(...a2);
    assertEqVecArr(simdfunc(v1, v2), ref);
}

function compareU(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I8x16 + I8x16CHK + U8x16 + U8x16I8x16 +
                                      `var fut = u8x16.${opname};` +
                                      'function f(v1, v2) { v1 = i8x16chk(v1); v2 = i8x16chk(v2); return fut(u8x16i8x16(v1), u8x16i8x16(v2)); } return f'), this);
    let a1 = [  -1,2,  -3,-128,0x7f,6,-7, 8,-9, 10,-11, 12,-13,-14,-15, -16].map(x => x << 24 >>> 24);
    let a2 = [-128,2,-128,0x7f,   0,0, 8,-9,10,-11, 12,-13,-14,-15,-16,  -1].map(x => x << 24 >>> 24);
    let ref = zipmap(a1, a2, lanefunc);
    let v1 = SIMD.Int8x16(...a1);
    let v2 = SIMD.Int8x16(...a2);
    assertEqVecArr(simdfunc(v1, v2), ref);
}

compareI("equal", (x,y) => x == y);
compareU("equal", (x,y) => x == y);
compareI("notEqual", (x,y) => x != y);
compareU("notEqual", (x,y) => x != y);
compareI("lessThan", (x,y) => x < y);
compareU("lessThan", (x,y) => x < y);
compareI("lessThanOrEqual", (x,y) => x <= y);
compareU("lessThanOrEqual", (x,y) => x <= y);
compareI("greaterThan", (x,y) => x > y);
compareU("greaterThan", (x,y) => x > y);
compareI("greaterThanOrEqual", (x,y) => x >= y);
compareU("greaterThanOrEqual", (x,y) => x >= y);
