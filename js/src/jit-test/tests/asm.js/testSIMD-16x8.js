load(libdir + "asm.js");
load(libdir + "simd.js");
load(libdir + "asserts.js");

// Set to true to see more JS debugging spew.
const DEBUG = false;

if (!isSimdAvailable()) {
    DEBUG && print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

// Tests for 16x8 SIMD types: Int16x8, Uint16x8, Bool16x8.

const I16x8 = 'var i16x8 = glob.SIMD.Int16x8;'
const I16x8CHK = 'var i16x8chk = i16x8.check;'
const I16x8EXT = 'var i16x8ext = i16x8.extractLane;'
const I16x8REP = 'var i16x8rep = i16x8.replaceLane;'
const I16x8U16x8 = 'var i16x8u16x8 = i16x8.fromUint16x8Bits;'

const U16x8 = 'var u16x8 = glob.SIMD.Uint16x8;'
const U16x8CHK = 'var u16x8chk = u16x8.check;'
const U16x8EXT = 'var u16x8ext = u16x8.extractLane;'
const U16x8REP = 'var u16x8rep = u16x8.replaceLane;'
const U16x8I16x8 = 'var u16x8i16x8 = u16x8.fromInt16x8Bits;'

const B16x8 = 'var b16x8 = glob.SIMD.Bool16x8;'
const B16x8CHK = 'var b16x8chk = b16x8.check;'
const B16x8EXT = 'var b16x8ext = b16x8.extractLane;'
const B16x8REP = 'var b16x8rep = b16x8.replaceLane;'

const INT16_MAX = 0x7fff
const INT16_MIN = -0x10000
const UINT16_MAX = 0xffff

// Linking
assertEq(asmLink(asmCompile('glob', USE_ASM + I16x8 + "function f() {} return f"), {SIMD:{Int16x8: SIMD.Int16x8}})(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + U16x8 + "function f() {} return f"), {SIMD:{Uint16x8: SIMD.Uint16x8}})(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + B16x8 + "function f() {} return f"), {SIMD:{Bool16x8: SIMD.Bool16x8}})(), undefined);

// Local variable of Int16x8 type.
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=Int16x8(1,2,3,4,5,6,7,8);} return f");
assertAsmTypeFail('glob', USE_ASM + I16x8 + "function f() {var x=i16x8;} return f");
assertAsmTypeFail('glob', USE_ASM + I16x8 + "function f() {var x=i16x8();} return f");
assertAsmTypeFail('glob', USE_ASM + I16x8 + "function f() {var x=i16x8(1);} return f");
assertAsmTypeFail('glob', USE_ASM + I16x8 + "function f() {var x=i16x8(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + I16x8 + "function f() {var x=i16x8(1,2,3,4,5,6,7,8.0);} return f");
assertAsmTypeFail('glob', USE_ASM + I16x8 + "function f() {var x=i16x8(1,2,3,4,5,6,7,8,9);} return f");
assertAsmTypeFail('glob', USE_ASM + I16x8 + "function f() {var x=i16x8(1,2,3,4,5,6,7,8|0);} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + I16x8 + "function f() {var x=i16x8(1,2,3,4,5,6,7,8);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + I16x8 + "function f() {var x=i16x8(1,2,3,4,5,6,7," + (INT16_MAX + 1) + ");} return f"), this)(), undefined);

// Local variable of Uint16x8 type.
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=Uint16x8(1,2,3,4,5,6,7,8);} return f");
assertAsmTypeFail('glob', USE_ASM + U16x8 + "function f() {var x=u16x8;} return f");
assertAsmTypeFail('glob', USE_ASM + U16x8 + "function f() {var x=u16x8();} return f");
assertAsmTypeFail('glob', USE_ASM + U16x8 + "function f() {var x=u16x8(1);} return f");
assertAsmTypeFail('glob', USE_ASM + U16x8 + "function f() {var x=u16x8(1,2,3,4);} return f");
assertAsmTypeFail('glob', USE_ASM + U16x8 + "function f() {var x=u16x8(1,2,3,4,5,6,7,8.0);} return f");
assertAsmTypeFail('glob', USE_ASM + U16x8 + "function f() {var x=u16x8(1,2,3,4,5,6,7,8,9);} return f");
assertAsmTypeFail('glob', USE_ASM + U16x8 + "function f() {var x=u16x8(1,2,3,4,5,6,7,8|0);} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + U16x8 + "function f() {var x=u16x8(1,2,3,4,5,6,7,8);} return f"), this)(), undefined);
assertEq(asmLink(asmCompile('glob', USE_ASM + U16x8 + "function f() {var x=u16x8(1,2,3,4,5,6,7," + (UINT16_MAX + 1) + ");} return f"), this)(), undefined);

// Local variable of Bool16x8 type.
assertAsmTypeFail('glob', USE_ASM + "function f() {var x=Bool16x8(1,0,0,0, 0,0,0,0);} return f");
assertAsmTypeFail('glob', USE_ASM + B16x8 + "function f() {var x=b16x8;} return f");
assertAsmTypeFail('glob', USE_ASM + B16x8 + "function f() {var x=b16x8();} return f");
assertAsmTypeFail('glob', USE_ASM + B16x8 + "function f() {var x=b16x8(1);} return f");
assertAsmTypeFail('glob', USE_ASM + B16x8 + "function f() {var x=b16x8(1,0,0,0);} return f");
assertAsmTypeFail('glob', USE_ASM + B16x8 + "function f() {var x=b16x8(1,0,0,0, 0,0,0,1.0);} return f");
assertAsmTypeFail('glob', USE_ASM + B16x8 + "function f() {var x=b16x8(1,0,0,0, 0,0,0,0|0);} return f");
assertAsmTypeFail('glob', USE_ASM + B16x8 + "function f() {var x=b16x8(1,0,0,0, 0,0,0,0, 1);} return f");
assertEq(asmLink(asmCompile('glob', USE_ASM + B16x8 + "function f() {var x=b16x8(1,0,0,0, 0,-1,-2,0);} return f"), this)(), undefined);

// Only signed Int16x8 allowed as return value.
assertEqVecArr(asmLink(asmCompile('glob', USE_ASM + I16x8 + "function f() {return i16x8(1,2,3,4,5,6,7,8);} return f"), this)(),
           [1, 2, 3, 4, 5, 6, 7, 8]);
assertEqVecArr(asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK + "function f() {return i16x8chk(i16x8(1,2,3,32771,5,6,7,8));} return f"), this)(),
           [1, 2, 3, -32765, 5, 6, 7, 8]);
assertAsmTypeFail('glob', USE_ASM + U16x8 + "function f() {return u16x8(1,2,3,4,5,6,7,8);} return f");
assertAsmTypeFail('glob', USE_ASM + U16x8 + U16x8CHK + "function f() {return u16x8chk(u16x8(1,2,3,4,5,6,7,8));} return f");

// Test splat.
function splat(x) {
    let r = []
    for (let i = 0; i < 8; i++)
        r.push(x);
    return r
}

splatB = asmLink(asmCompile('glob', USE_ASM + B16x8 +
                            'var splat = b16x8.splat;' +
                            'function f(x) { x = x|0; return splat(x); } return f'), this);
assertEqVecArr(splatB(true), splat(true));
assertEqVecArr(splatB(false), splat(false));


splatB0 = asmLink(asmCompile('glob', USE_ASM + B16x8 +
                             'var splat = b16x8.splat;' +
                             'function f() { var x = 0; return splat(x); } return f'), this);
assertEqVecArr(splatB0(), splat(false));
splatB1 = asmLink(asmCompile('glob', USE_ASM + B16x8 +
                             'var splat = b16x8.splat;' +
                             'function f() { var x = 1; return splat(x); } return f'), this);
assertEqVecArr(splatB1(), splat(true));

splatI = asmLink(asmCompile('glob', USE_ASM + I16x8 +
                            'var splat = i16x8.splat;' +
                            'function f(x) { x = x|0; return splat(x); } return f'), this);
for (let x of [0, 1, -1, 0x12345, 0x1234, -1000, -1000000]) {
    assertEqVecArr(splatI(x), splat(x << 16 >> 16));
}

splatIc = asmLink(asmCompile('glob', USE_ASM + I16x8 +
                             'var splat = i16x8.splat;' +
                             'function f() { var x = 100; return splat(x); } return f'), this);
assertEqVecArr(splatIc(), splat(100))

splatU = asmLink(asmCompile('glob', USE_ASM + U16x8 + I16x8 + I16x8U16x8 +
                            'var splat = u16x8.splat;' +
                            'function f(x) { x = x|0; return i16x8u16x8(splat(x)); } return f'), this);
for (let x of [0, 1, -1, 0x12345, 0x1234, -1000, -1000000]) {
    assertEqVecArr(SIMD.Uint16x8.fromInt16x8Bits(splatI(x)), splat(x << 16 >>> 16));
}

splatUc = asmLink(asmCompile('glob', USE_ASM + U16x8 + I16x8 + I16x8U16x8 +
                             'var splat = u16x8.splat;' +
                             'function f() { var x = 200; return i16x8u16x8(splat(x)); } return f'), this);
assertEqVecArr(SIMD.Uint16x8.fromInt16x8Bits(splatUc()), splat(200))


// Test extractLane.
//
// The lane index must be a literal int, and we generate different code for
// different lanes.
function extractI(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8EXT +
                            `function f() {var x=i16x8(${a.join(',')}); return i16x8ext(x, ${i})|0; } return f`), this)();
}
a = [-1,2,-3,4,-5,6,-7,-8];
for (var i = 0; i < 8; i++)
  assertEq(extractI(a, i), a[i]);
a = a.map(x => -x);
for (var i = 0; i < 8; i++)
  assertEq(extractI(a, i), a[i]);

function extractU(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + U16x8 + U16x8EXT +
                            `function f() {var x=u16x8(${a.join(',')}); return u16x8ext(x, ${i})|0; } return f`), this)();
}
a = [1,255,12,13,14,150,200,3];
for (var i = 0; i < 8; i++)
  assertEq(extractU(a, i), a[i]);
a = a.map(x => UINT16_MAX-x);
for (var i = 0; i < 8; i++)
  assertEq(extractU(a, i), a[i]);

function extractB(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + B16x8 + B16x8EXT +
                            `function f() {var x=b16x8(${a.join(',')}); return b16x8ext(x, ${i})|0; } return f`), this)();
}
a = [1,1,0,1, 1,0,0,0];
for (var i = 0; i < 8; i++)
  assertEq(extractB(a, i), a[i]);
a = a.map(x => 1-x);
for (var i = 0; i < 8; i++)
  assertEq(extractB(a, i), a[i]);

// Test replaceLane.
function replaceI(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8REP +
                            `function f(v) {v=v|0; var x=i16x8(${a.join(',')}); return i16x8rep(x,${i},v); } return f`), this);
}
a = [-1,2,-3,4,-5,6,-7,-9];
for (var i = 0; i < 8; i++) {
    var f = replaceI(a, i);
    var b = a.slice(0);
    b[i] = -20;
    assertEqVecArr(f(-20), b);
}

function replaceU(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + U16x8 + U16x8REP + I16x8 + I16x8U16x8 +
                            `function f(v) {v=v|0; var x=u16x8(${a.join(',')}); return i16x8u16x8(u16x8rep(x,${i},v)); } return f`), this);
}
a = [65000-1,2,65000-3,4,65000-5,6,65000-7,65000-9];
for (var i = 0; i < 8; i++) {
    var rawf = replaceU(a, i);
    var f = x => SIMD.Uint16x8.fromInt16x8Bits(rawf(x))
    var b = a.slice(0);
    b[i] = 1000;
    assertEqVecArr(f(1000), b);
}

function replaceB(a, i) {
  return asmLink(asmCompile('glob', USE_ASM + B16x8 + B16x8REP +
                            `function f(v) {v=v|0; var x=b16x8(${a.join(',')}); return b16x8rep(x,${i},v); } return f`), this);
}
a = [1,1,0,1,1,0,0,0];
for (var i = 0; i < 8; i++) {
    var f = replaceB(a, i);
    var b = a.slice(0);
    let v = 1 - a[i];
    b[i] = v;
    assertEqVecArr(f(v), b.map(x => !!x));
}


// Test select.
selectI = asmLink(asmCompile('glob', USE_ASM + I16x8 + B16x8 + B16x8CHK +
                             'var select = i16x8.select;' +
                             'var a = i16x8(-1,2,-3,4,-5, 6,-7, 8);' +
                             'var b = i16x8( 5,6, 7,8, 9,10,11,12);' +
                             'function f(x) { x = b16x8chk(x); return select(x, a, b); } return f'), this);
assertEqVecArr(selectI(SIMD.Bool16x8( 0,0, 1,0, 1,1, 1, 0)),
                                    [ 5,6,-3,8,-5,6,-7,12]);

selectU = asmLink(asmCompile('glob', USE_ASM + I16x8 + B16x8 + B16x8CHK + U16x8 + I16x8U16x8 + U16x8I16x8 +
                             'var select = u16x8.select;' +
                             'var a = i16x8(-1,2,-3,4,-5, 6,-7, 8);' +
                             'var b = i16x8( 5,6, 7,8, 9,10,11,12);' +
                             'function f(x) { x = b16x8chk(x); return i16x8u16x8(select(x, u16x8i16x8(a), u16x8i16x8(b))); } return f'), this);
assertEqVecArr(selectU(SIMD.Bool16x8( 0,0, 1,0, 1,1, 1, 0)),
                                    [ 5,6,-3,8,-5,6,-7,12]);

// Test swizzle.
function swizzle(vec, lanes) {
    let r = [];
    for (let i = 0; i < 8; i++)
        r.push(vec[lanes[i]]);
    return r;
}

function swizzleI(lanes) {
    let asm = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK +
                                 'var swz = i16x8.swizzle;' +
                                 `function f(a) { a = i16x8chk(a); return swz(a, ${lanes.join()}); } return f`), this);
    let a1 = [    -1,2,    -3,0x8000,0x7f,6,-7, 8].map(x => x << 16 >> 16);
    let a2 = [0x8000,2,0x8000,0x7fff,   0,0, 8,-9].map(x => x << 16 >> 16);
    let v1 = SIMD.Int16x8(...a1);
    let v2 = SIMD.Int16x8(...a2);
    assertEqVecArr(asm(v1), swizzle(a1, lanes));
    assertEqVecArr(asm(v2), swizzle(a2, lanes));
}

swizzleI([3, 4, 7, 1, 4, 3, 1, 2]);
swizzleI([0, 0, 0, 0, 0, 0, 0, 0]);
swizzleI([7, 7, 7, 7, 7, 7, 7, 7]);

function swizzleU(lanes) {
    let asm = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK + U16x8 + U16x8I16x8 + I16x8U16x8 +
                                 'var swz = u16x8.swizzle;' +
                                 `function f(a) { a = i16x8chk(a); return i16x8u16x8(swz(u16x8i16x8(a), ${lanes.join()})); } return f`), this);
    let a1 = [    -1,2,    -3,0x8000,0x7f,6,-7, 8].map(x => x << 16 >> 16);
    let a2 = [0x8000,2,0x8000,0x7fff,   0,0, 8,-9].map(x => x << 16 >> 16);
    let v1 = SIMD.Int16x8(...a1);
    let v2 = SIMD.Int16x8(...a2);
    assertEqVecArr(asm(v1), swizzle(a1, lanes));
    assertEqVecArr(asm(v2), swizzle(a2, lanes));
}

swizzleU([3, 4, 7, 1, 4, 3, 1, 2]);
swizzleU([0, 0, 0, 0, 0, 0, 0, 0]);
swizzleU([7, 7, 7, 7, 7, 7, 7, 7]);

// Out-of-range lane indexes.
assertAsmTypeFail('glob', USE_ASM + I16x8 + 'var swz = i16x8.swizzle; ' +
                  'function f() { var x=i16x8(0,0,0,0,0,0,0,0); swz(x,1,2,3,4,5,6,7,8); } return f');
assertAsmTypeFail('glob', USE_ASM + U16x8 + 'var swz = u16x8.swizzle; ' +
                  'function f() { var x=u16x8(0,0,0,0,0,0,0,0); swz(x,1,2,3,4,5,6,7,8); } return f');
// Missing lane indexes.
assertAsmTypeFail('glob', USE_ASM + I16x8 + 'var swz = i16x8.swizzle; ' +
                  'function f() { var x=i16x8(0,0,0,0,0,0,0,0); swz(x,1,2,3,4,5,6,7); } return f');
assertAsmTypeFail('glob', USE_ASM + U16x8 + 'var swz = u16x8.swizzle; ' +
                  'function f() { var x=u16x8(0,0,0,0,0,0,0,0); swz(x,1,2,3,4,5,6,7); } return f');


// Test shuffle.
function shuffle(vec1, vec2, lanes) {
    let r = [];
    let vec = vec1.concat(vec2)
    for (let i = 0; i < 8; i++)
        r.push(vec[lanes[i]]);
    return r;
}

function shuffleI(lanes) {
    let asm = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK +
                                 'var shuf = i16x8.shuffle;' +
                                 `function f(a1, a2) { a1 = i16x8chk(a1); a2 = i16x8chk(a2); return shuf(a1, a2, ${lanes.join()}); } return f`), this);
    let a1 = [    -1,2,    -3,0x8000,0x7f,6,-7, 8].map(x => x << 16 >> 16);
    let a2 = [0x8000,2,0x8000,0x7fff,   0,0, 8,-9].map(x => x << 16 >> 16);
    let v1 = SIMD.Int16x8(...a1);
    let v2 = SIMD.Int16x8(...a2);
    assertEqVecArr(asm(v1, v2), shuffle(a1, a2, lanes));
}

function shuffleU(lanes) {
    let asm = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK + U16x8 + U16x8I16x8 + I16x8U16x8 +
                                 'var shuf = u16x8.shuffle;' +
                                 'function f(a1, a2) { a1 = i16x8chk(a1); a2 = i16x8chk(a2); ' +
                                 `return i16x8u16x8(shuf(u16x8i16x8(a1), u16x8i16x8(a2), ${lanes.join()})); } return f`), this);
    let a1 = [    -1,2,    -3,0x8000,0x7f,6,-7, 8].map(x => x << 16 >> 16);
    let a2 = [0x8000,2,0x8000,0x7fff,   0,0, 8,-9].map(x => x << 16 >> 16);
    let v1 = SIMD.Int16x8(...a1);
    let v2 = SIMD.Int16x8(...a2);
    assertEqVecArr(asm(v1, v2), shuffle(a1, a2, lanes));
}

shuffleI([0, 0, 0, 0, 0, 0, 0, 0])
shuffleI([15, 15, 15, 15, 15, 15, 15, 15])
shuffleI([6, 2, 0, 14, 6, 10, 11, 1])

shuffleU([7, 7, 7, 7, 7, 7, 7, 7])
shuffleU([8, 15, 15, 15, 15, 15, 15, 15])
shuffleU([6, 2, 0, 14, 6, 10, 11, 1])

// Test unary operators.
function unaryI(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK +
                                      `var fut = i16x8.${opname};` +
                                      'function f(v) { v = i16x8chk(v); return fut(v); } return f'), this);
    let a = [65000-1,2,65000-3,4,65000-5,6,65000-7,65000-9];
    let v = SIMD.Int16x8(...a);
    assertEqVecArr(simdfunc(v), a.map(lanefunc));
}

function unaryU(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + U16x8 + I16x8 + I16x8CHK + U16x8I16x8 + I16x8U16x8 +
                                      `var fut = u16x8.${opname};` +
                                      'function f(v) { v = i16x8chk(v); return i16x8u16x8(fut(u16x8i16x8(v))); } return f'), this);
    let a = [65000-1,2,65000-3,4,65000-5,6,65000-7,65000-9];
    let v = SIMD.Int16x8(...a);
    assertEqVecArr(SIMD.Uint16x8.fromInt16x8Bits(simdfunc(v)), a.map(lanefunc));
}

function unaryB(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + B16x8 + B16x8CHK +
                                      `var fut = b16x8.${opname};` +
                                      'function f(v) { v = b16x8chk(v); return fut(v); } return f'), this);
    let a = [1,1,0,1,1,0,0,0];
    let v = SIMD.Bool16x8(...a);
    assertEqVecArr(simdfunc(v), a.map(lanefunc));
}

unaryI('not', x => ~x << 16 >> 16);
unaryU('not', x => ~x << 16 >>> 16);
unaryB('not', x => !x);
unaryI('neg', x => -x << 16 >> 16);
unaryU('neg', x => -x << 16 >>> 16);


// Test binary operators.
function zipmap(a1, a2, f) {
    assertEq(a1.length, a2.length);
    let r = [];
    for (var i = 0; i < a1.length; i++)
        r.push(f(a1[i], a2[i]));
    return r
}

function binaryI(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK +
                                      `var fut = i16x8.${opname};` +
                                      'function f(v1, v2) { v1 = i16x8chk(v1); v2 = i16x8chk(v2); return fut(v1, v2); } return f'), this);
    let a1 = [    -1,2,    -3,0x8000,0x7f,6,-7, 8].map(x => x << 16 >> 16);
    let a2 = [0x8000,2,0x8000,0x7fff,   0,0, 8,-9].map(x => x << 16 >> 16);
    let ref = zipmap(a1, a2, lanefunc);
    let v1 = SIMD.Int16x8(...a1);
    let v2 = SIMD.Int16x8(...a2);
    assertEqVecArr(simdfunc(v1, v2), ref);
}

function binaryU(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + U16x8 + I16x8 + I16x8CHK + U16x8I16x8 + I16x8U16x8 +
                                      `var fut = u16x8.${opname};` +
                                      'function f(v1, v2) { v1 = i16x8chk(v1); v2 = i16x8chk(v2); return i16x8u16x8(fut(u16x8i16x8(v1), u16x8i16x8(v2))); } return f'), this);
    let a1 = [    -1,2,    -3,0x8000,0x7f,6,-7, 8].map(x => x << 16 >>> 16);
    let a2 = [0x8000,2,0x8000,0x7fff,   0,0, 8,-9].map(x => x << 16 >>> 16);
    let ref = zipmap(a1, a2, lanefunc);
    let v1 = SIMD.Int16x8(...a1);
    let v2 = SIMD.Int16x8(...a2);
    let res = SIMD.Uint16x8.fromInt16x8Bits(simdfunc(v1, v2));
    assertEqVecArr(res, ref);
}

function binaryB(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + B16x8 + B16x8CHK +
                                      `var fut = b16x8.${opname};` +
                                      'function f(v1, v2) { v1 = b16x8chk(v1); v2 = b16x8chk(v2); return fut(v1, v2); } return f'), this);
    let a = [1,1,0,1,1,0,0,0];
    let v = SIMD.Bool16x8(...a);
    assertEqVecArr(simdfunc(v), a.map(lanefunc));
}

binaryI('add', (x, y) => (x + y) << 16 >> 16);
binaryI('sub', (x, y) => (x - y) << 16 >> 16);
binaryI('mul', (x, y) => (x * y) << 16 >> 16);
binaryU('add', (x, y) => (x + y) << 16 >>> 16);
binaryU('sub', (x, y) => (x - y) << 16 >>> 16);
binaryU('mul', (x, y) => (x * y) << 16 >>> 16);

binaryI('and', (x, y) => (x & y) << 16 >> 16);
binaryI('or',  (x, y) => (x | y) << 16 >> 16);
binaryI('xor', (x, y) => (x ^ y) << 16 >> 16);
binaryU('and', (x, y) => (x & y) << 16 >>> 16);
binaryU('or',  (x, y) => (x | y) << 16 >>> 16);
binaryU('xor', (x, y) => (x ^ y) << 16 >>> 16);

function sat(x, lo, hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x
}
function isat(x) { return sat(x, -32768, 32767); }
function usat(x) { return sat(x, 0, 0xffff); }

binaryI('addSaturate', (x, y) => isat(x + y))
binaryI('subSaturate', (x, y) => isat(x - y))
binaryU('addSaturate', (x, y) => usat(x + y))
binaryU('subSaturate', (x, y) => usat(x - y))


// Test shift operators.
function zip1map(a, s, f) {
    return a.map(x => f(x, s));
}

function shiftI(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK +
                                      `var fut = i16x8.${opname};` +
                                      'function f(v, s) { v = i16x8chk(v); s = s|0; return fut(v, s); } return f'), this);
    let a = [-1,2,-3,0x80,0x7f,6,0x8000,0x7fff];
    let v = SIMD.Int16x8(...a);
    for (let s of [0, 1, 2, 6, 7, 8, 9, 10, 16, 255, -1, -8, -7, -1000]) {
        let ref = zip1map(a, s, lanefunc);
        // 1. Test dynamic shift amount.
        assertEqVecArr(simdfunc(v, s), ref);

        // 2. Test constant shift amount.
        let cstf = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK +
                                      `var fut = i16x8.${opname};` +
                                      `function f(v) { v = i16x8chk(v); return fut(v, ${s}); } return f`), this);
        assertEqVecArr(cstf(v, s), ref);
    }
}

function shiftU(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + U16x8 + I16x8 + I16x8CHK + U16x8I16x8 + I16x8U16x8 +
                                      `var fut = u16x8.${opname};` +
                                      'function f(v, s) { v = i16x8chk(v); s = s|0; return i16x8u16x8(fut(u16x8i16x8(v), s)); } return f'), this);
    let a = [-1,2,-3,0x80,0x7f,6,0x8000,0x7fff];
    let v = SIMD.Int16x8(...a);
    for (let s of [0, 1, 2, 6, 7, 8, 9, 10, 16, 255, -1, -8, -7, -1000]) {
        let ref = zip1map(a, s, lanefunc);
        // 1. Test dynamic shift amount.
        assertEqVecArr(SIMD.Uint16x8.fromInt16x8Bits(simdfunc(v, s)), ref);

        // 2. Test constant shift amount.
        let cstf = asmLink(asmCompile('glob', USE_ASM + U16x8 + I16x8 + I16x8CHK + U16x8I16x8 + I16x8U16x8 +
                                      `var fut = u16x8.${opname};` +
                                      `function f(v) { v = i16x8chk(v); return i16x8u16x8(fut(u16x8i16x8(v), ${s})); } return f`), this);
        assertEqVecArr(SIMD.Uint16x8.fromInt16x8Bits(cstf(v, s)), ref);
    }
}

shiftI('shiftLeftByScalar', (x,s) => (x << (s & 15)) << 16 >> 16);
shiftU('shiftLeftByScalar', (x,s) => (x << (s & 15)) << 16 >>> 16);
shiftI('shiftRightByScalar', (x,s) => ((x << 16 >> 16) >> (s & 15)) << 16 >> 16);
shiftU('shiftRightByScalar', (x,s) => ((x << 16 >>> 16) >>> (s & 15)) << 16 >>> 16);


// Comparisons.
function compareI(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK +
                                      `var fut = i16x8.${opname};` +
                                      'function f(v1, v2) { v1 = i16x8chk(v1); v2 = i16x8chk(v2); return fut(v1, v2); } return f'), this);
    let a1 = [    -1,2,    -3,0x8000,0x7f,6,-7, 8].map(x => x << 16 >> 16);
    let a2 = [0x8000,2,0x8000,0x7fff,   0,0, 8,-9].map(x => x << 16 >> 16);
    let ref = zipmap(a1, a2, lanefunc);
    let v1 = SIMD.Int16x8(...a1);
    let v2 = SIMD.Int16x8(...a2);
    assertEqVecArr(simdfunc(v1, v2), ref);
}

function compareU(opname, lanefunc) {
    let simdfunc = asmLink(asmCompile('glob', USE_ASM + I16x8 + I16x8CHK + U16x8 + U16x8I16x8 +
                                      `var fut = u16x8.${opname};` +
                                      'function f(v1, v2) { v1 = i16x8chk(v1); v2 = i16x8chk(v2); return fut(u16x8i16x8(v1), u16x8i16x8(v2)); } return f'), this);
    let a1 = [    -1,2,    -3,0x8000,0x7f,6,-7, 8].map(x => x << 16 >>> 16);
    let a2 = [0x8000,2,0x8000,0x7fff,   0,0, 8,-9].map(x => x << 16 >>> 16);
    let ref = zipmap(a1, a2, lanefunc);
    let v1 = SIMD.Int16x8(...a1);
    let v2 = SIMD.Int16x8(...a2);
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
