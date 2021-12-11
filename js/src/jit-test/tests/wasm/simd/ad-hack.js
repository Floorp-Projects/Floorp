// |jit-test| skip-if: !wasmSimdEnabled(); include:../tests/wasm/simd/ad-hack-preamble.js

// Ad-hoc test cases used during development.  Generally these are ordered from
// easier toward harder.
//
// The test cases here are usually those that require some special processing.
// Simple binary operators (v128 x v128 -> v128) and unary operators (v128 ->
// v128) are tested in ad-hack-simple-binops*.js and ad-hack-simple-unops.js.

// v128.store
// oob store
// v128.const

for ( let offset of [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]) {
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "f") (param $loc i32)
      (v128.store offset=${offset} (local.get $loc) (v128.const i32x4 ${1+offset} 2 3 ${4+offset*2}))))`);
    var mem8 = new Uint8Array(ins.exports.mem.buffer);
    ins.exports.f(160);
    assertSame(getUnaligned(mem8, 4, 160 + offset, 4), [1+offset, 2, 3, 4+offset*2]);

    // OOB write should trap
    assertErrorMessage(() => ins.exports.f(65536-15),
                       WebAssembly.RuntimeError,
                       /index out of bounds/)

    // Ensure that OOB writes don't write anything: moved to simd-partial-oob-store.js
}

// v128.load
// oob load
// v128.store
// temp register

for ( let offset of [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]) {
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "copy") (param $dest i32) (param $src i32)
      (v128.store (local.get $dest) (v128.load offset=${offset} (local.get $src)))))`);
    var mem32 = new Uint32Array(ins.exports.mem.buffer);
    var mem8 = new Uint8Array(ins.exports.mem.buffer);
    setUnaligned(mem8, 4, 4*4 + offset, [8+offset, 10, 12, 14+offset*2]);
    ins.exports.copy(40*4, 4*4);
    assertSame(get(mem32, 40, 4), [8+offset, 10, 12, 14+offset*2]);
    assertErrorMessage(() => ins.exports.copy(40*4, 65536-15),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
}

// call [with register params]
// parameters [in registers]
// return [with register values]
// locals
//
// local.get
// local.set
// v128.const
// v128.store

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $g (param $param v128) (result v128)
      (local $tmp v128)
      (local.set $tmp (local.get $param))
      (local.get $tmp))
    (func (export "f")
      (v128.store (i32.const 160) (call $g (v128.const i32x4 1 2 3 4)))))`);
var mem = new Uint32Array(ins.exports.mem.buffer);
ins.exports.f();
assertSame(get(mem, 40, 4), [1, 2, 3, 4]);

// Same test but with local.tee

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $g (param $param v128) (result v128)
      (local $tmp v128)
      (local.tee $tmp (local.get $param)))
    (func (export "f")
      (v128.store (i32.const 160) (call $g (v128.const i32x4 1 2 3 4)))))`);
var mem = new Uint32Array(ins.exports.mem.buffer);
ins.exports.f();
assertSame(get(mem, 40, 4), [1, 2, 3, 4]);

// Locals that end up on the stack.  Try to create unaligned placement (in the
// baseline compiler anyway) by inserting i32 locals before or after and
// inbetween the v128 ones and by having so many locals that we run out of
// registers.

var nlocals = 64;
for ( let start of [0, 1]) {
    let decl = "";
    let set = "";
    let sum = "(v128.const i32x4 0 0 0 0)";
    var res = [0,0,0,0];
    var locno = start;
    for ( let i=start ; i < start + nlocals ; i++ ) {
        decl += "(local v128) ";
        set += `(local.set ${locno} (v128.const i32x4 ${i} ${i+1} ${i+2} ${i+3})) `;
        sum = `(i32x4.add ${sum} (local.get ${locno}))`;
        locno++;
        res[0] += i;
        res[1] += i+1;
        res[2] += i+2;
        res[3] += i+3;
        if ((i % 5) == 3) {
            decl += "(local i32) ";
            locno++;
        }
    }
    if (start)
        decl = "(local i32) " + decl;
    else
        decl += "(local i32) ";
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $g (result v128)
      ${decl}
      ${set}
      ${sum})
    (func (export "f")
      (v128.store (i32.const 160) (call $g))))`);

    var mem = new Uint32Array(ins.exports.mem.buffer);
    ins.exports.f();
    assertSame(get(mem, 40, 4), res);
}

// Ditto parameters.  This is like the case above but values are passed rather
// than set.
//
// call
// call_indirect

var nlocals = 64;
for ( let start of [0, 1]) {
    let decl = "";
    let pass = "";
    let sum = "(v128.const i32x4 0 0 0 0)";
    var res = [0,0,0,0];
    var locno = start;
    for ( let i=start ; i < start + nlocals ; i++ ) {
        decl += "(param v128) ";
        pass += `(v128.const i32x4 ${i} ${i+1} ${i+2} ${i+3}) `;
        sum = `(i32x4.add ${sum} (local.get ${locno}))`;
        locno++;
        res[0] += i;
        res[1] += i+1;
        res[2] += i+2;
        res[3] += i+3;
        if ((i % 5) == 3) {
            decl += "(param i32) ";
            pass += "(i32.const 0) ";
            locno++;
        }
    }
    if (start) {
        decl = "(param i32) " + decl;
        pass = "(i32.const 0) " + pass;
    } else {
        decl += "(param i32) ";
        pass += "(i32.const 0) ";
    }
    var txt = `
  (module
    (memory (export "mem") 1 1)
    (type $t1 (func ${decl} (result v128)))
    (table funcref (elem $h))
    (func $g ${decl} (result v128)
      ${sum})
    (func (export "f1")
      (v128.store (i32.const 160) (call $g ${pass})))
    (func $h ${decl} (result v128)
      ${sum})
    (func (export "f2")
      (v128.store (i32.const 512) (call_indirect (type $t1) ${pass} (i32.const 0)))))`;
    var ins = wasmEvalText(txt);

    var mem = new Uint32Array(ins.exports.mem.buffer);
    ins.exports.f1();
    assertSame(get(mem, 40, 4), res);
    ins.exports.f2();
    assertSame(get(mem, 128, 4), res);
}

// Widening integer dot product

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "run")
      (v128.store (i32.const 0)
        (i32x4.dot_i16x8_s (v128.load (i32.const 16)) (v128.load (i32.const 32))))))`);

var xs = [5, 1, -4, 2, 20, -15, 12, 3];
var ys = [6, 0, -7, 3, 8, -1, -3, 7];
var ans = [xs[0]*ys[0] + xs[1]*ys[1],
           xs[2]*ys[2] + xs[3]*ys[3],
           xs[4]*ys[4] + xs[5]*ys[5],
           xs[6]*ys[6] + xs[7]*ys[7]];

var mem16 = new Int16Array(ins.exports.mem.buffer);
var mem32 = new Int32Array(ins.exports.mem.buffer);
set(mem16, 8, xs);
set(mem16, 16, ys);
ins.exports.run();
var result = get(mem32, 0, 4);
assertSame(result, ans);

// Splat, with and without constants (different code paths in ion)

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "splat_i8x16") (param $src i32)
      (v128.store (i32.const 0) (i8x16.splat (local.get $src))))
    (func (export "csplat_i8x16")
      (v128.store (i32.const 0) (i8x16.splat (i32.const 37))))
    (func (export "splat_i16x8") (param $src i32)
      (v128.store (i32.const 0) (i16x8.splat (local.get $src))))
    (func (export "csplat_i16x8")
      (v128.store (i32.const 0) (i16x8.splat (i32.const 1175))))
    (func (export "splat_i32x4") (param $src i32)
      (v128.store (i32.const 0) (i32x4.splat (local.get $src))))
    (func (export "csplat_i32x4")
      (v128.store (i32.const 0) (i32x4.splat (i32.const 127639))))
    (func (export "splat_i64x2") (param $src i64)
      (v128.store (i32.const 0) (i64x2.splat (local.get $src))))
    (func (export "csplat_i64x2")
      (v128.store (i32.const 0) (i64x2.splat (i64.const 0x1234_5678_4365))))
    (func (export "splat_f32x4") (param $src f32)
      (v128.store (i32.const 0) (f32x4.splat (local.get $src))))
    (func (export "csplat_f32x4")
      (v128.store (i32.const 0) (f32x4.splat (f32.const 9121.25))))
    (func (export "splat_f64x2") (param $src f64)
      (v128.store (i32.const 0) (f64x2.splat (local.get $src))))
    (func (export "csplat_f64x2")
      (v128.store (i32.const 0) (f64x2.splat (f64.const 26789.125))))
)`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
ins.exports.splat_i8x16(3);
assertSame(get(mem8, 0, 16), iota(16).map(_=>3));
ins.exports.csplat_i8x16();
assertSame(get(mem8, 0, 16), iota(16).map(_=>37));

var mem16 = new Uint16Array(ins.exports.mem.buffer);
ins.exports.splat_i16x8(976);
assertSame(get(mem16, 0, 8), iota(8).map(_=>976));
ins.exports.csplat_i16x8();
assertSame(get(mem16, 0, 8), iota(8).map(_=>1175));

var mem32 = new Uint32Array(ins.exports.mem.buffer);
ins.exports.splat_i32x4(147812);
assertSame(get(mem32, 0, 4), [147812, 147812, 147812, 147812]);
ins.exports.csplat_i32x4();
assertSame(get(mem32, 0, 4), [127639, 127639, 127639, 127639]);

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
ins.exports.splat_i64x2(147812n);
assertSame(get(mem64, 0, 2), [147812, 147812]);
ins.exports.csplat_i64x2();
assertSame(get(mem64, 0, 2), [0x1234_5678_4365n, 0x1234_5678_4365n]);

var memf32 = new Float32Array(ins.exports.mem.buffer);
ins.exports.splat_f32x4(147812.5);
assertSame(get(memf32, 0, 4), [147812.5, 147812.5, 147812.5, 147812.5]);
ins.exports.csplat_f32x4();
assertSame(get(memf32, 0, 4), [9121.25, 9121.25, 9121.25, 9121.25]);

var memf64 = new Float64Array(ins.exports.mem.buffer);
ins.exports.splat_f64x2(147812.5);
assertSame(get(memf64, 0, 2), [147812.5, 147812.5]);
ins.exports.csplat_f64x2();
assertSame(get(memf64, 0, 2), [26789.125, 26789.125]);

// AnyTrue.  Ion constant folds, so test that too.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "anytrue_i8x16") (result i32)
      (v128.any_true (v128.load (i32.const 16))))
    (func (export "true_anytrue_i8x16") (result i32)
      (v128.any_true (v128.const i8x16 0 0 8 0 0 0 0 0 0 0 0 0 0 0 0 0)))
    (func (export "false_anytrue_i8x16") (result i32)
      (v128.any_true (v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0))))`);

var mem = new Uint8Array(ins.exports.mem.buffer);
set(mem, 16, iota(16).map((_) => 0));
assertEq(ins.exports.anytrue_i8x16(), 0);

for ( let dope of [1, 7, 32, 195 ] ) {
    set(mem, 16, iota(16).map((x) => x == 7 ? dope : 0));
    assertEq(ins.exports.anytrue_i8x16(), 1);
}

assertEq(ins.exports.true_anytrue_i8x16(), 1);
assertEq(ins.exports.false_anytrue_i8x16(), 0);

// AllTrue.  Ion constant folds, so test that too.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "alltrue_i8x16") (result i32)
      (i8x16.all_true (v128.load (i32.const 16))))
    (func (export "true_alltrue_i8x16") (result i32)
      (i8x16.all_true (v128.const i8x16 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16)))
    (func (export "false_alltrue_i8x16") (result i32)
      (i8x16.all_true (v128.const i8x16 1 2 3 4 5 6 0 8 9 10 11 12 13 14 15 16)))
    (func (export "alltrue_i16x8") (result i32)
      (i16x8.all_true (v128.load (i32.const 16))))
    (func (export "true_alltrue_i16x8") (result i32)
      (i16x8.all_true (v128.const i16x8 1 2 3 4 5 6 7 8)))
    (func (export "false_alltrue_i16x8") (result i32)
      (i16x8.all_true (v128.const i16x8 1 2 3 4 5 0 7 8)))
    (func (export "alltrue_i32x4") (result i32)
      (i32x4.all_true (v128.load (i32.const 16))))
    (func (export "true_alltrue_i32x4") (result i32)
      (i32x4.all_true (v128.const i32x4 1 2 3 4)))
    (func (export "false_alltrue_i32x4") (result i32)
      (i32x4.all_true (v128.const i32x4 1 2 3 0))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var mem16 = new Uint16Array(ins.exports.mem.buffer);
var mem32 = new Uint32Array(ins.exports.mem.buffer);

set(mem8, 16, iota(16).map((_) => 0));
assertEq(ins.exports.alltrue_i8x16(), 0);
assertEq(ins.exports.alltrue_i16x8(), 0);
assertEq(ins.exports.alltrue_i32x4(), 0);

set(mem8, 16, iota(16).map((_) => 1));
assertEq(ins.exports.alltrue_i8x16(), 1);

set(mem16, 8, iota(8).map((_) => 1));
assertEq(ins.exports.alltrue_i16x8(), 1);

set(mem32, 4, iota(4).map((_) => 1));
assertEq(ins.exports.alltrue_i32x4(), 1);

for ( let dope of [1, 7, 32, 195 ] ) {
    set(mem8, 16, iota(16).map((x) => x == 7 ? 0 : dope));
    assertEq(ins.exports.alltrue_i8x16(), 0);

    set(mem16, 8, iota(8).map((x) => x == 4 ? 0 : dope));
    assertEq(ins.exports.alltrue_i16x8(), 0);

    set(mem32, 4, iota(4).map((x) => x == 2 ? 0 : dope));
    assertEq(ins.exports.alltrue_i32x4(), 0);
}

assertEq(ins.exports.true_alltrue_i8x16(), 1);
assertEq(ins.exports.false_alltrue_i8x16(), 0);
assertEq(ins.exports.true_alltrue_i16x8(), 1);
assertEq(ins.exports.false_alltrue_i16x8(), 0);
assertEq(ins.exports.true_alltrue_i32x4(), 1);
assertEq(ins.exports.false_alltrue_i32x4(), 0);

// Bitmask.  Ion constant folds, so test that too.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "bitmask_i8x16") (result i32)
      (i8x16.bitmask (v128.load (i32.const 16))))
    (func (export "const_bitmask_i8x16") (result i32)
      (i8x16.bitmask (v128.const i8x16 0x80 0x7f 0xff 0x33 0x42 0x98 0x01 0x00
                                       0x31 0xcc 0xdd 0x12 0xf0 0x40 0x02 0xa0)))
    (func (export "bitmask_i16x8") (result i32)
      (i16x8.bitmask (v128.load (i32.const 16))))
    (func (export "const_bitmask_i16x8") (result i32)
      (i16x8.bitmask (v128.const i16x8 0x7f80 0xff33 0x9842 0x0001 0xcc31 0x12dd 0x40f0 0xa002)))
    (func (export "bitmask_i32x4") (result i32)
      (i32x4.bitmask (v128.load (i32.const 16))))
    (func (export "const_bitmask_i32x4") (result i32)
      (i32x4.bitmask (v128.const i32x4 0xff337f80 0x00019842 0xcc3112dd 0xa00240f0))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var mem16 = new Uint16Array(ins.exports.mem.buffer);
var mem32 = new Uint32Array(ins.exports.mem.buffer);

set(mem8, 16, iota(16).map((_) => 0));
assertEq(ins.exports.bitmask_i8x16(), 0);
assertEq(ins.exports.bitmask_i16x8(), 0);
assertEq(ins.exports.bitmask_i32x4(), 0);

set(mem8, 16, iota(16).map((_) => 0x80));
assertEq(ins.exports.bitmask_i8x16(), 0xFFFF);

set(mem8, 16, iota(16).map((_) => 0x7F));
assertEq(ins.exports.bitmask_i8x16(), 0);

set(mem8, 16, iota(16).map((i) => popcount(i) == 1 ? 0x80 : 0));
assertEq(ins.exports.bitmask_i8x16(), (1 << 1) | (1 << 2) | (1 << 4) | (1 << 8));

assertEq(ins.exports.const_bitmask_i8x16(), 0x9625);

set(mem16, 8, iota(8).map((i) => 0x8000))
assertEq(ins.exports.bitmask_i16x8(), 0xFF)

set(mem16, 8, iota(8).map((i) => 0x7FFF))
assertEq(ins.exports.bitmask_i16x8(), 0)

set(mem16, 8, iota(8).map((i) => popcount(i) == 1 ? 0x8000 : 0))
assertEq(ins.exports.bitmask_i16x8(), (1 << 1) | (1 << 2) | (1 << 4));

assertEq(ins.exports.const_bitmask_i16x8(), 0x96);

set(mem32, 4, iota(4).map((_) => 0x80000000))
assertEq(ins.exports.bitmask_i32x4(), 0xF);

set(mem32, 4, iota(4).map((_) => 0x7FFFFFFF))
assertEq(ins.exports.bitmask_i32x4(), 0);

set(mem32, 4, iota(4).map((i) => popcount(i) == 1 ? 0x80000000 : 0))
assertEq(ins.exports.bitmask_i32x4(), (1 << 1) | (1 << 2));

assertEq(ins.exports.const_bitmask_i32x4(), 0xd);

// Shifts
//
// lhs is v128 in memory
// rhs is i32 (passed directly)
// result is v128 in memory

function shr(count, width) {
    return (v) => {
        if (count == 0)
            return v;
        if (width == 64) {
            if (v < 0) {
                // This basically mirrors what the SIMD code does, so if there's
                // a bug there then there's a bug here too.  Seems OK though.
                let s = 0x1_0000_0000_0000_0000n + BigInt(v);
                let t = s / (1n << BigInt(count));
                let u = ((1n << BigInt(count)) - 1n) * (2n ** BigInt(64-count));
                let w = t + u;
                return w - 0x1_0000_0000_0000_0000n;
            }
            return BigInt(v) / (1n << BigInt(count));
        } else {
            let mask = (width == 32) ? -1 : ((1 << width) - 1);
            return (sign_extend(v, width) >> count) & mask;
        }
    }
}

function shru(count, width) {
    if (width == 64) {
        return (v) => {
            if (count == 0)
                return v;
            if (v < 0) {
                v = 0x1_0000_0000_0000_0000n + BigInt(v);
            }
            return BigInt(v) / (1n << BigInt(count));
        }
    } else {
        return (v) => {
            let mask = (width == 32) ? -1 : ((1 << width) - 1);
            return (v >>> count) & mask;
        }
    }
}

var constantI8Shifts = "";
for ( let i of iota(10).concat([-7]) ) {
    constantI8Shifts += `
    (func (export "shl_i8x16_${i}")
      (v128.store (i32.const 0) (i8x16.shl (v128.load (i32.const 16)) (i32.const ${i}))))
    (func (export "shr_i8x16_${i}")
      (v128.store (i32.const 0) (i8x16.shr_s (v128.load (i32.const 16)) (i32.const ${i}))))
    (func (export "shr_u8x16_${i}")
      (v128.store (i32.const 0) (i8x16.shr_u (v128.load (i32.const 16)) (i32.const ${i}))))`;
}

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "shl_i8x16") (param $count i32)
      (v128.store (i32.const 0) (i8x16.shl (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_i8x16") (param $count i32)
      (v128.store (i32.const 0) (i8x16.shr_s (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_u8x16") (param $count i32)
      (v128.store (i32.const 0) (i8x16.shr_u (v128.load (i32.const 16)) (local.get $count))))
    ${constantI8Shifts}
    (func (export "shl_i16x8") (param $count i32)
      (v128.store (i32.const 0) (i16x8.shl (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shl_i16x8_3")
      (v128.store (i32.const 0) (i16x8.shl (v128.load (i32.const 16)) (i32.const 3))))
    (func (export "shl_i16x8_15")
      (v128.store (i32.const 0) (i16x8.shl (v128.load (i32.const 16)) (i32.const 15))))
    (func (export "shl_i16x8_16")
      (v128.store (i32.const 0) (i16x8.shl (v128.load (i32.const 16)) (i32.const 16))))
    (func (export "shl_i16x8_-15")
      (v128.store (i32.const 0) (i16x8.shl (v128.load (i32.const 16)) (i32.const -15))))
    (func (export "shr_i16x8") (param $count i32)
      (v128.store (i32.const 0) (i16x8.shr_s (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_i16x8_3")
      (v128.store (i32.const 0) (i16x8.shr_s (v128.load (i32.const 16)) (i32.const 3))))
    (func (export "shr_i16x8_15")
      (v128.store (i32.const 0) (i16x8.shr_s (v128.load (i32.const 16)) (i32.const 15))))
    (func (export "shr_i16x8_16")
      (v128.store (i32.const 0) (i16x8.shr_s (v128.load (i32.const 16)) (i32.const 16))))
    (func (export "shr_i16x8_-15")
      (v128.store (i32.const 0) (i16x8.shr_s (v128.load (i32.const 16)) (i32.const -15))))
    (func (export "shr_u16x8") (param $count i32)
      (v128.store (i32.const 0) (i16x8.shr_u (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_u16x8_3")
      (v128.store (i32.const 0) (i16x8.shr_u (v128.load (i32.const 16)) (i32.const 3))))
    (func (export "shr_u16x8_15")
      (v128.store (i32.const 0) (i16x8.shr_u (v128.load (i32.const 16)) (i32.const 15))))
    (func (export "shr_u16x8_16")
      (v128.store (i32.const 0) (i16x8.shr_u (v128.load (i32.const 16)) (i32.const 16))))
    (func (export "shr_u16x8_-15")
      (v128.store (i32.const 0) (i16x8.shr_u (v128.load (i32.const 16)) (i32.const -15))))
    (func (export "shl_i32x4") (param $count i32)
      (v128.store (i32.const 0) (i32x4.shl (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shl_i32x4_12")
      (v128.store (i32.const 0) (i32x4.shl (v128.load (i32.const 16)) (i32.const 12))))
    (func (export "shl_i32x4_31")
      (v128.store (i32.const 0) (i32x4.shl (v128.load (i32.const 16)) (i32.const 31))))
    (func (export "shl_i32x4_32")
      (v128.store (i32.const 0) (i32x4.shl (v128.load (i32.const 16)) (i32.const 32))))
    (func (export "shl_i32x4_-27")
      (v128.store (i32.const 0) (i32x4.shl (v128.load (i32.const 16)) (i32.const -27))))
    (func (export "shr_i32x4") (param $count i32)
      (v128.store (i32.const 0) (i32x4.shr_s (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_i32x4_12")
      (v128.store (i32.const 0) (i32x4.shr_s (v128.load (i32.const 16)) (i32.const 12))))
    (func (export "shr_i32x4_31")
      (v128.store (i32.const 0) (i32x4.shr_s (v128.load (i32.const 16)) (i32.const 31))))
    (func (export "shr_i32x4_32")
      (v128.store (i32.const 0) (i32x4.shr_s (v128.load (i32.const 16)) (i32.const 32))))
    (func (export "shr_i32x4_-27")
      (v128.store (i32.const 0) (i32x4.shr_s (v128.load (i32.const 16)) (i32.const -27))))
    (func (export "shr_u32x4") (param $count i32)
      (v128.store (i32.const 0) (i32x4.shr_u (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_u32x4_12")
      (v128.store (i32.const 0) (i32x4.shr_u (v128.load (i32.const 16)) (i32.const 12))))
    (func (export "shr_u32x4_31")
      (v128.store (i32.const 0) (i32x4.shr_u (v128.load (i32.const 16)) (i32.const 31))))
    (func (export "shr_u32x4_32")
      (v128.store (i32.const 0) (i32x4.shr_u (v128.load (i32.const 16)) (i32.const 32))))
    (func (export "shr_u32x4_-27")
      (v128.store (i32.const 0) (i32x4.shr_u (v128.load (i32.const 16)) (i32.const -27))))
    (func (export "shl_i64x2") (param $count i32)
      (v128.store (i32.const 0) (i64x2.shl (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shl_i64x2_27")
      (v128.store (i32.const 0) (i64x2.shl (v128.load (i32.const 16)) (i32.const 27))))
    (func (export "shl_i64x2_63")
      (v128.store (i32.const 0) (i64x2.shl (v128.load (i32.const 16)) (i32.const 63))))
    (func (export "shl_i64x2_64")
      (v128.store (i32.const 0) (i64x2.shl (v128.load (i32.const 16)) (i32.const 64))))
    (func (export "shl_i64x2_-231")
      (v128.store (i32.const 0) (i64x2.shl (v128.load (i32.const 16)) (i32.const -231))))
    (func (export "shr_i64x2") (param $count i32)
      (v128.store (i32.const 0) (i64x2.shr_s (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_i64x2_27")
      (v128.store (i32.const 0) (i64x2.shr_s (v128.load (i32.const 16)) (i32.const 27))))
    (func (export "shr_i64x2_45")
      (v128.store (i32.const 0) (i64x2.shr_s (v128.load (i32.const 16)) (i32.const 45))))
    (func (export "shr_i64x2_63")
      (v128.store (i32.const 0) (i64x2.shr_s (v128.load (i32.const 16)) (i32.const 63))))
    (func (export "shr_i64x2_64")
      (v128.store (i32.const 0) (i64x2.shr_s (v128.load (i32.const 16)) (i32.const 64))))
    (func (export "shr_i64x2_-231")
      (v128.store (i32.const 0) (i64x2.shr_s (v128.load (i32.const 16)) (i32.const -231))))
    (func (export "shr_i64x2_-1")
      (v128.store (i32.const 0) (i64x2.shr_s (v128.load (i32.const 16)) (i32.const -1))))
    (func (export "shr_u64x2") (param $count i32)
      (v128.store (i32.const 0) (i64x2.shr_u (v128.load (i32.const 16)) (local.get $count))))
    (func (export "shr_u64x2_27")
      (v128.store (i32.const 0) (i64x2.shr_u (v128.load (i32.const 16)) (i32.const 27))))
    (func (export "shr_u64x2_63")
      (v128.store (i32.const 0) (i64x2.shr_u (v128.load (i32.const 16)) (i32.const 63))))
    (func (export "shr_u64x2_64")
      (v128.store (i32.const 0) (i64x2.shr_u (v128.load (i32.const 16)) (i32.const 64))))
    (func (export "shr_u64x2_-231")
      (v128.store (i32.const 0) (i64x2.shr_u (v128.load (i32.const 16)) (i32.const -231)))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var as = [1, 2, 4, 8, 16, 32, 64, 128, 129, 130, 132, 136, 144, 160, 192, 255];

set(mem8, 16, as);

for (let [meth,op] of [["shl_i8x16",shl], ["shr_i8x16",shr], ["shr_u8x16",shru]]) {
    for ( let i=0 ; i < 8 ; i++ ) {
        ins.exports[meth](i);
        assertSame(get(mem8, 0, 16), as.map(op(i, 8)))
        ins.exports[meth + "_" + i]();
        assertSame(get(mem8, 0, 16), as.map(op(i, 8)))
    }

    ins.exports[meth](1);
    let a = get(mem8, 0, 16);
    ins.exports[meth](9);
    let b = get(mem8, 0, 16);
    assertSame(a, b);
    ins.exports[meth](-7);
    let c = get(mem8, 0, 16);
    assertSame(a, c);

    ins.exports[meth + "_1"]();
    let x = get(mem8, 0, 16);
    ins.exports[meth + "_9"]();
    let y = get(mem8, 0, 16);
    ins.exports[meth + "_-7"]();
    let z = get(mem8, 0, 16);
    assertSame(x, y);
    assertSame(x, z);
}

var mem16 = new Uint16Array(ins.exports.mem.buffer);
var as = [1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000];
set(mem16, 8, as)

ins.exports.shl_i16x8(2);
var res = get(mem16, 0, 8);
assertSame(res, as.map(shl(2, 16)))

ins.exports.shl_i16x8(18);      // Masked count
assertSame(get(mem16, 0, 8), res);

ins.exports.shl_i16x8(-14);      // Masked count
assertSame(get(mem16, 0, 8), res);

for ( let shift of [3, 15, 16, -15] ) {
    ins.exports["shl_i16x8_" + shift]();
    assertSame(get(mem16, 0, 8), as.map(shl(shift & 15, 16)))
}

ins.exports.shr_i16x8(1);
var res = get(mem16, 0, 8);
assertSame(res, as.map(shr(1, 16)))

ins.exports.shr_i16x8(17);      // Masked count
assertSame(get(mem16, 0, 8), res);

ins.exports.shr_i16x8(-15);      // Masked count
assertSame(get(mem16, 0, 8), res);

for ( let shift of [3, 15, 16, -15] ) {
    ins.exports["shr_i16x8_" + shift]();
    assertSame(get(mem16, 0, 8), as.map(shr(shift & 15, 16)))
}

ins.exports.shr_u16x8(1);
var res = get(mem16, 0, 8);
assertSame(res, as.map(shru(1, 16)))

ins.exports.shr_u16x8(17);      // Masked count
assertSame(get(mem16, 0, 8), res);

ins.exports.shr_u16x8(-15);      // Masked count
assertSame(get(mem16, 0, 8), res);

for ( let shift of [3, 15, 16, -15] ) {
    ins.exports["shr_u16x8_" + shift]();
    assertSame(get(mem16, 0, 8), as.map(shru(shift & 15, 16)))
}

var mem32 = new Uint32Array(ins.exports.mem.buffer);
var as = [5152, 6768, 7074, 800811];

set(mem32, 4, as)
ins.exports.shl_i32x4(2);
var res = get(mem32, 0, 4);
assertSame(res, as.map(shl(2, 32)))

ins.exports.shl_i32x4(34);      // Masked count
assertSame(get(mem32, 0, 4), res);

ins.exports.shl_i32x4(-30);      // Masked count
assertSame(get(mem32, 0, 4), res);

for ( let shift of [12, 31, 32, -27] ) {
    ins.exports["shl_i32x4_" + shift]();
    assertSame(get(mem32, 0, 4), as.map(shl(shift & 31, 32)).map(x => x>>>0))
}

ins.exports.shr_i32x4(1);
var res = get(mem32, 0, 4);
assertSame(res, as.map(shr(1, 32)))

ins.exports.shr_i32x4(33);      // Masked count
assertSame(get(mem32, 0, 4), res);

ins.exports.shr_i32x4(-31);      // Masked count
assertSame(get(mem32, 0, 4), res);

for ( let shift of [12, 31, 32, -27] ) {
    ins.exports["shr_i32x4_" + shift]();
    assertSame(get(mem32, 0, 4), as.map(shr(shift & 31, 32)))
}

ins.exports.shr_u32x4(1);
var res = get(mem32, 0, 4);
assertSame(res, as.map(shru(1, 32)))

ins.exports.shr_u32x4(33);      // Masked count
assertSame(get(mem32, 0, 4), res);

ins.exports.shr_u32x4(-31);      // Masked count
assertSame(get(mem32, 0, 4), res);

for ( let shift of [12, 31, 32, -27] ) {
    ins.exports["shr_u32x4_" + shift]();
    assertSame(get(mem32, 0, 4), as.map(shru(shift & 31, 32)))
}

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
var as = [50515253, -616263];

set(mem64, 2, as)
ins.exports.shl_i64x2(2);
var res = get(mem64, 0, 2);
assertSame(res, as.map(shl(2, 64)))

ins.exports.shl_i64x2(66);      // Masked count
assertSame(get(mem64, 0, 2), res);

ins.exports.shl_i64x2(-62);      // Masked count
assertSame(get(mem64, 0, 2), res);

for ( let shift of [27, 63, 64, -231] ) {
    ins.exports["shl_i64x2_" + shift]();
    assertSame(get(mem64, 0, 2), as.map(shl(shift & 63, 64)))
}

ins.exports.shr_u64x2(1);
var res = get(mem64, 0, 2);
assertSame(res, as.map(shru(1, 64)))

ins.exports.shr_u64x2(65);      // Masked count
assertSame(get(mem64, 0, 2), res);

ins.exports.shr_u64x2(-63);      // Masked count
assertSame(get(mem64, 0, 2), res);

for ( let shift of [27, 63, 64, -231] ) {
    ins.exports["shr_u64x2_" + shift]();
    assertSame(get(mem64, 0, 2), as.map(shru(shift & 63, 64)))
}

ins.exports.shr_i64x2(2);
var res = get(mem64, 0, 2);
assertSame(res, as.map(shr(2, 64)))

ins.exports.shr_i64x2(66);      // Masked count
assertSame(get(mem64, 0, 2), res);

ins.exports.shr_i64x2(-62);      // Masked count
assertSame(get(mem64, 0, 2), res);

// The ion code generator has multiple paths here, for < 32 and >= 32
for ( let shift of [27, 45, 63, 64, -1, -231] ) {
    ins.exports["shr_i64x2_" + shift]();
    assertSame(get(mem64, 0, 2), as.map(shr(shift & 63, 64)))
}

// Narrow

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "narrow_i16x8_s")
      (v128.store (i32.const 0) (i8x16.narrow_i16x8_s (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
    (func (export "narrow_i16x8_u")
      (v128.store (i32.const 0) (i8x16.narrow_i16x8_u (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
    (func (export "narrow_i32x4_s")
      (v128.store (i32.const 0) (i16x8.narrow_i32x4_s (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
    (func (export "narrow_i32x4_u")
      (v128.store (i32.const 0) (i16x8.narrow_i32x4_u (v128.load (i32.const 16)) (v128.load (i32.const 32))))))`);

var mem8 = new Int8Array(ins.exports.mem.buffer);
var mem8u = new Uint8Array(ins.exports.mem.buffer);
var mem16 = new Int16Array(ins.exports.mem.buffer);
var mem16u = new Uint16Array(ins.exports.mem.buffer);
var mem32 = new Int32Array(ins.exports.mem.buffer);

var as = [1, 267, 3987, 14523, 32768, 3, 312, 4876].map((x) => sign_extend(x, 16));
var bs = [2, 312, 4876, 15987, 33777, 1, 267, 3987].map((x) => sign_extend(x, 16));

set(mem16, 8, as);
set(mem16, 16, bs);

ins.exports.narrow_i16x8_s();
var cs = as.concat(...bs).map((x) => signed_saturate(x, 8));
assertSame(get(mem8, 0, 16), cs);

ins.exports.narrow_i16x8_u();
var cs = as.concat(...bs).map((x) => unsigned_saturate(x, 8));
assertSame(get(mem8u, 0, 16), cs);

var xs = [1, 3987, 14523, 32768].map((x) => x << 16).map((x) => sign_extend(x, 32));
var ys = [2, 4876, 15987, 33777].map((x) => x << 16).map((x) => sign_extend(x, 32));

set(mem32, 4, xs);
set(mem32, 8, ys);

ins.exports.narrow_i32x4_s();
var cs = xs.concat(...ys).map((x) => signed_saturate(x, 16));
assertSame(get(mem16, 0, 8), cs);

ins.exports.narrow_i32x4_u();
var cs = xs.concat(...ys).map((x) => unsigned_saturate(x, 16));
assertSame(get(mem16u, 0, 8), cs);

// Extend low/high

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "extend_low_i8x16_s")
      (v128.store (i32.const 0) (i16x8.extend_low_i8x16_s (v128.load (i32.const 16)))))
    (func (export "extend_high_i8x16_s")
      (v128.store (i32.const 0) (i16x8.extend_high_i8x16_s (v128.load (i32.const 16)))))
    (func (export "extend_low_i8x16_u")
      (v128.store (i32.const 0) (i16x8.extend_low_i8x16_u (v128.load (i32.const 16)))))
    (func (export "extend_high_i8x16_u")
      (v128.store (i32.const 0) (i16x8.extend_high_i8x16_u (v128.load (i32.const 16)))))
    (func (export "extend_low_i16x8_s")
      (v128.store (i32.const 0) (i32x4.extend_low_i16x8_s (v128.load (i32.const 16)))))
    (func (export "extend_high_i16x8_s")
      (v128.store (i32.const 0) (i32x4.extend_high_i16x8_s (v128.load (i32.const 16)))))
    (func (export "extend_low_i16x8_u")
      (v128.store (i32.const 0) (i32x4.extend_low_i16x8_u (v128.load (i32.const 16)))))
    (func (export "extend_high_i16x8_u")
      (v128.store (i32.const 0) (i32x4.extend_high_i16x8_u (v128.load (i32.const 16))))))`);

var mem16 = new Int16Array(ins.exports.mem.buffer);
var mem16u = new Uint16Array(ins.exports.mem.buffer);
var mem8 =  new Int8Array(ins.exports.mem.buffer);
var as = [0, 1, 192, 3, 205, 5, 6, 133, 8, 9, 129, 11, 201, 13, 14, 255];

set(mem8, 16, as);

ins.exports.extend_low_i8x16_s();
assertSame(get(mem16, 0, 8), iota(8).map((n) => sign_extend(as[n], 8)));

ins.exports.extend_high_i8x16_s();
assertSame(get(mem16, 0, 8), iota(8).map((n) => sign_extend(as[n+8], 8)));

ins.exports.extend_low_i8x16_u();
assertSame(get(mem16u, 0, 8), iota(8).map((n) => zero_extend(as[n], 8)));

ins.exports.extend_high_i8x16_u();
assertSame(get(mem16u, 0, 8), iota(8).map((n) => zero_extend(as[n+8], 8)));

var mem32 = new Int32Array(ins.exports.mem.buffer);
var mem32u = new Uint32Array(ins.exports.mem.buffer);

var as = [0, 1, 192, 3, 205, 5, 6, 133].map((x) => x << 8);

set(mem16, 8, as);

ins.exports.extend_low_i16x8_s();
assertSame(get(mem32, 0, 4), iota(4).map((n) => sign_extend(as[n], 16)));

ins.exports.extend_high_i16x8_s();
assertSame(get(mem32, 0, 4), iota(4).map((n) => sign_extend(as[n+4], 16)));

ins.exports.extend_low_i16x8_u();
assertSame(get(mem32u, 0, 4), iota(4).map((n) => zero_extend(as[n], 16)));

ins.exports.extend_high_i16x8_u();
assertSame(get(mem32u, 0, 4), iota(4).map((n) => zero_extend(as[n+4], 16)));


// Extract lane.  Ion constant folds, so test that too.
//
// operand is v128 in memory (or constant)
// lane index is immediate so we're testing something randomish but not zero
// result is scalar (returned directly)

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "extract_i8x16_9") (result i32)
      (i8x16.extract_lane_s 9 (v128.load (i32.const 16))))
    (func (export "const_extract_i8x16_9") (result i32)
      (i8x16.extract_lane_s 9 (v128.const i8x16 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16)))
    (func (export "extract_u8x16_6") (result i32)
      (i8x16.extract_lane_u 6 (v128.load (i32.const 16))))
    (func (export "const_extract_u8x16_9") (result i32)
      (i8x16.extract_lane_u 9 (v128.const i8x16 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16)))
    (func (export "extract_i16x8_5") (result i32)
      (i16x8.extract_lane_s 5 (v128.load (i32.const 16))))
    (func (export "const_extract_i16x8_5") (result i32)
      (i16x8.extract_lane_s 5 (v128.const i16x8 -1 -2 -3 -4 -5 -6 -7 -8)))
    (func (export "extract_u16x8_3") (result i32)
      (i16x8.extract_lane_u 3 (v128.load (i32.const 16))))
    (func (export "const_extract_u16x8_3") (result i32)
      (i16x8.extract_lane_u 3 (v128.const i16x8 -1 -2 -3 -4 -5 -6 -7 -8)))
    (func (export "extract_i32x4_2") (result i32)
      (i32x4.extract_lane 2 (v128.load (i32.const 16))))
    (func (export "const_extract_i32x4_2") (result i32)
      (i32x4.extract_lane 2 (v128.const i32x4 -1 -2 -3 -4)))
    (func (export "extract_i64x2_1") (result i64)
      (i64x2.extract_lane 1 (v128.load (i32.const 16))))
    (func (export "const_extract_i64x2_1") (result i64)
      (i64x2.extract_lane 1 (v128.const i64x2 -1 -2)))
    (func (export "extract_f32x4_2") (result f32)
      (f32x4.extract_lane 2 (v128.load (i32.const 16))))
    (func (export "const_extract_f32x4_2") (result f32)
      (f32x4.extract_lane 2 (v128.const f32x4 -1 -2 -3 -4)))
    (func (export "extract_f64x2_1") (result f64)
      (f64x2.extract_lane 1 (v128.load (i32.const 16))))
    (func (export "const_extract_f64x2_1") (result f64)
      (f64x2.extract_lane 1 (v128.const f64x2 -1 -2))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
var bs = as.map((x) => -x);

set(mem8, 16, as)
assertEq(ins.exports.extract_i8x16_9(), as[9]);

set(mem8, 16, bs)
assertEq(ins.exports.extract_u8x16_6(), 256 - as[6]);

assertEq(ins.exports.const_extract_i8x16_9(), -10);
assertEq(ins.exports.const_extract_u8x16_9(), 256-10);

var mem16 = new Uint16Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4, 5, 6, 7, 8];
var bs = as.map((x) => -x);

set(mem16, 8, as)
assertEq(ins.exports.extract_i16x8_5(), as[5]);

set(mem16, 8, bs)
assertEq(ins.exports.extract_u16x8_3(), 65536 - as[3]);

assertEq(ins.exports.const_extract_i16x8_5(), -6);
assertEq(ins.exports.const_extract_u16x8_3(), 65536-4);

var mem32 = new Uint32Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4];

set(mem32, 4, as)
assertEq(ins.exports.extract_i32x4_2(), as[2]);

assertEq(ins.exports.const_extract_i32x4_2(), -3);

var mem32 = new Float32Array(ins.exports.mem.buffer);
var as = [1.5, 2.5, 3.5, 4.5];

set(mem32, 4, as)
assertEq(ins.exports.extract_f32x4_2(), as[2]);

assertEq(ins.exports.const_extract_f32x4_2(), -3);

var mem64 = new Float64Array(ins.exports.mem.buffer);
var as = [1.5, 2.5];

set(mem64, 2, as)
assertEq(ins.exports.extract_f64x2_1(), as[1]);

assertEq(ins.exports.const_extract_f64x2_1(), -2);

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
var as = [12345, 67890];

set(mem64, 2, as)
assertSame(ins.exports.extract_i64x2_1(), as[1]);

assertEq(ins.exports.const_extract_i64x2_1(), -2n);

// Replace lane
//
// operand 1 is v128 in memory
// operand 2 is immediate scalar
// lane index is immediate so we're testing something randomish but not zero
// (note though that fp operations have special cases for zero)
// result is v128 in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "replace_i8x16_9") (param $value i32)
      (v128.store (i32.const 0)
        (i8x16.replace_lane 9 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_i16x8_5") (param $value i32)
      (v128.store (i32.const 0)
        (i16x8.replace_lane 5 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_i32x4_3") (param $value i32)
      (v128.store (i32.const 0)
        (i32x4.replace_lane 3 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_i64x2_1") (param $value i64)
      (v128.store (i32.const 0)
        (i64x2.replace_lane 1 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_f32x4_0") (param $value f32)
      (v128.store (i32.const 0)
        (f32x4.replace_lane 0 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_f32x4_3") (param $value f32)
      (v128.store (i32.const 0)
        (f32x4.replace_lane 3 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_f64x2_0") (param $value f64)
      (v128.store (i32.const 0)
        (f64x2.replace_lane 0 (v128.load (i32.const 16)) (local.get $value))))
    (func (export "replace_f64x2_1") (param $value f64)
      (v128.store (i32.const 0)
        (f64x2.replace_lane 1 (v128.load (i32.const 16)) (local.get $value)))))`);


var mem8 = new Uint8Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

set(mem8, 16, as)
ins.exports.replace_i8x16_9(42);
assertSame(get(mem8, 0, 16), upd(as, 9, 42));

var mem16 = new Uint16Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4, 5, 6, 7, 8];

set(mem16, 8, as)
ins.exports.replace_i16x8_5(42);
assertSame(get(mem16, 0, 8), upd(as, 5, 42));

var mem32 = new Uint32Array(ins.exports.mem.buffer);
var as = [1, 2, 3, 4];

set(mem32, 4, as)
ins.exports.replace_i32x4_3(42);
assertSame(get(mem32, 0, 4), upd(as, 3, 42));

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
var as = [1, 2];

set(mem64, 2, as)
ins.exports.replace_i64x2_1(42n);
assertSame(get(mem64, 0, 2), upd(as, 1, 42));

var mem32 = new Float32Array(ins.exports.mem.buffer);
var as = [1.5, 2.5, 3.5, 4.5];

set(mem32, 4, as)
ins.exports.replace_f32x4_0(42.5);
assertSame(get(mem32, 0, 4), upd(as, 0, 42.5));

set(mem32, 4, as)
ins.exports.replace_f32x4_3(42.5);
assertSame(get(mem32, 0, 4), upd(as, 3, 42.5));

var mem64 = new Float64Array(ins.exports.mem.buffer);
var as = [1.5, 2.5];

set(mem64, 2, as)
ins.exports.replace_f64x2_0(42.5);
assertSame(get(mem64, 0, 2), upd(as, 0, 42.5));

set(mem64, 2, as)
ins.exports.replace_f64x2_1(42.5);
assertSame(get(mem64, 0, 2), upd(as, 1, 42.5));

// Load and splat
//
// Operand is memory address of scalar
// Result is v128 in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "load_splat_v8x16") (param $addr i32)
      (v128.store (i32.const 0) (v128.load8_splat (local.get $addr))))
    (func (export "load_splat_v16x8") (param $addr i32)
      (v128.store (i32.const 0) (v128.load16_splat (local.get $addr))))
    (func (export "load_splat_v32x4") (param $addr i32)
      (v128.store (i32.const 0) (v128.load32_splat (local.get $addr))))
    (func (export "load_splat_v64x2") (param $addr i32)
      (v128.store (i32.const 0) (v128.load64_splat (local.get $addr)))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
mem8[37] = 42;
ins.exports.load_splat_v8x16(37);
assertSame(get(mem8, 0, 16), [42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42]);

var mem16 = new Uint16Array(ins.exports.mem.buffer);
mem16[37] = 69;
ins.exports.load_splat_v16x8(37*2);
assertSame(get(mem16, 0, 8), [69, 69, 69, 69, 69, 69, 69, 69]);

var mem32 = new Int32Array(ins.exports.mem.buffer);
mem32[37] = 83;
ins.exports.load_splat_v32x4(37*4);
assertSame(get(mem32, 0, 4), [83, 83, 83, 83]);

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
mem64[37] = 83n;
ins.exports.load_splat_v64x2(37*8);
assertSame(get(mem64, 0, 2), [83, 83]);

// Load and zero
//
// Operand is memory address of scalar
// Result is v128 in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "load32_zero") (param $addr i32)
      (v128.store (i32.const 0) (v128.load32_zero (local.get $addr))))
    (func (export "load64_zero") (param $addr i32)
      (v128.store (i32.const 0) (v128.load64_zero (local.get $addr)))))`);

var mem32 = new Int32Array(ins.exports.mem.buffer);
mem32[37] = 0x12345678;
mem32[38] = 0xffffffff;
mem32[39] = 0xfffffffe;
mem32[40] = 0xfffffffd;
ins.exports.load32_zero(37*4);
assertSame(get(mem32, 0, 4), [0x12345678, 0, 0, 0]);

var mem64 = new BigInt64Array(ins.exports.mem.buffer);
mem64[37] = 0x12345678abcdef01n;
mem64[38] = 0xffffffffffffffffn;
ins.exports.load64_zero(37*8);
assertSame(get(mem64, 0, 2), [0x12345678abcdef01n, 0n]);

// Load and extend
//
// Operand is memory address of 64-bit scalar representing 8, 4, or 2 values
// Result is v128 in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "load8x8_s") (param $addr i32)
      (v128.store (i32.const 0) (v128.load8x8_s (local.get $addr))))
    (func (export "load8x8_u") (param $addr i32)
      (v128.store (i32.const 0) (v128.load8x8_u (local.get $addr))))
    (func (export "load16x4_s") (param $addr i32)
      (v128.store (i32.const 0) (v128.load16x4_s (local.get $addr))))
    (func (export "load16x4_u") (param $addr i32)
      (v128.store (i32.const 0) (v128.load16x4_u (local.get $addr))))
    (func (export "load32x2_s") (param $addr i32)
      (v128.store (i32.const 0) (v128.load32x2_s (local.get $addr))))
    (func (export "load32x2_u") (param $addr i32)
      (v128.store (i32.const 0) (v128.load32x2_u (local.get $addr)))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var mem16s = new Int16Array(ins.exports.mem.buffer);
var mem16u = new Uint16Array(ins.exports.mem.buffer);
var mem32s = new Int32Array(ins.exports.mem.buffer);
var mem32u = new Uint32Array(ins.exports.mem.buffer);
var mem64s = new BigInt64Array(ins.exports.mem.buffer);
var mem64u = new BigUint64Array(ins.exports.mem.buffer);
var xs = [42, 129, 2, 212, 44, 27, 12, 199];
set(mem8, 48, xs);

ins.exports.load8x8_s(48);
assertSame(get(mem16s, 0, 8), xs.map((x) => sign_extend(x, 8)));

ins.exports.load8x8_u(48);
assertSame(get(mem16u, 0, 8), xs.map((x) => zero_extend(x, 8)));

var xs = [(42 << 8) | 129, (212 << 8) | 2, (44 << 8) | 27, (199 << 8) | 12];
set(mem16u, 24, xs);

ins.exports.load16x4_s(48);
assertSame(get(mem32s, 0, 4), xs.map((x) => sign_extend(x, 16)));

ins.exports.load16x4_u(48);
assertSame(get(mem32u, 0, 4), xs.map((x) => zero_extend(x, 16)));

var xs = [5, -8];
set(mem32u, 12, xs);

ins.exports.load32x2_s(48);
assertSame(get(mem64s, 0, 2), xs.map((x) => sign_extend(x, 32)));

ins.exports.load32x2_u(48);
assertSame(get(mem64s, 0, 2), xs.map((x) => zero_extend(x, 32)));

// Vector select
//
// Operands and results are all in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "bitselect_v128")
      (v128.store (i32.const 0)
        (v128.bitselect (v128.load (i32.const 16))
                        (v128.load (i32.const 32))
                        (v128.load (i32.const 48))))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
set(mem8, 16, iota(16).map((_) => 0xAA));
set(mem8, 32, iota(16).map((_) => 0x55));

set(mem8, 48, iota(16).map((_) => 0x99));
ins.exports.bitselect_v128();
assertSame(get(mem8, 0, 16), iota(16).map((_) => 0xCC));

set(mem8, 48, iota(16).map((_) => 0x77));
ins.exports.bitselect_v128();
assertSame(get(mem8, 0, 16), iota(16).map((_) => 0x22));

// Vector shuffle
//
// Operands and results are all in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    ;; the result interleaves the low eight bytes of the inputs
    (func (export "shuffle1")
      (v128.store (i32.const 0)
        (i8x16.shuffle 0 16 1 17 2 18 3 19 4 20 5 21 6 22 7 23
           (v128.load (i32.const 16))
           (v128.load (i32.const 32)))))
    ;; ditto the high eight bytes
    (func (export "shuffle2")
      (v128.store (i32.const 0)
        (i8x16.shuffle 8 24 9 25 10 26 11 27 12 28 13 29 14 30 15 31
           (v128.load (i32.const 16))
           (v128.load (i32.const 32))))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);
var xs = iota(16).map((n) => 0xA0 + n);
var ys = iota(16).map((n) => 0x50 + n);
set(mem8, 16, xs);
set(mem8, 32, ys);

ins.exports.shuffle1();
assertSame(get(mem8, 0, 16), iota(16).map((x) => ((x & 1) ? ys : xs)[x >>> 1]))

ins.exports.shuffle2();
assertSame(get(mem8, 0, 16), iota(32).map((x) => ((x & 1) ? ys : xs)[x >>> 1]).slice(16));

// Vector swizzle (variable permute).
//
// Case 1: Operands and results are all in memory

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "swizzle")
      (v128.store (i32.const 0)
        (i8x16.swizzle (v128.load (i32.const 16)) (v128.load (i32.const 32))))))`);

var mem8 = new Uint8Array(ins.exports.mem.buffer);

var xs = [100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115];
set(mem8, 16, xs);

set(mem8, 32, [1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14]);
ins.exports.swizzle();
assertSame(get(mem8, 0, 16), [101,100,103,102,105,104,107,106,109,108,111,110,113,112,115,114]);

set(mem8, 32, [9,8,11,10,13,12,16,14,1,0,3,2,5,192,7,6]);
ins.exports.swizzle();
assertSame(get(mem8, 0, 16), [109,108,111,110,113,112,0,114,101,100,103,102,105,0,107,106]);

// Case 2: The mask operand is a constant; the swizzle gets optimized into a
// shuffle (also see ion-analysis.js).

for ( let [mask, expected] of [[[1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14],
                                [101,100,103,102,105,104,107,106,109,108,111,110,113,112,115,114]],
                               [[9,8,11,10,13,12,16,14,1,0,3,2,5,192,7,6],
                                [109,108,111,110,113,112,0,114,101,100,103,102,105,0,107,106]]] ) {

    let ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "swizzle")
      (v128.store (i32.const 0)
        (i8x16.swizzle (v128.load (i32.const 16)) (v128.const i8x16 ${mask.join(' ')})))))
`);

    let mem8 = new Uint8Array(ins.exports.mem.buffer);
    set(mem8, 16, [100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115]);
    ins.exports.swizzle();
    assertSame(get(mem8, 0, 16), expected);
}

// Convert integer to floating point

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "convert_s")
      (v128.store (i32.const 0)
        (f32x4.convert_i32x4_s (v128.load (i32.const 16)))))
    (func (export "convert_u")
      (v128.store (i32.const 0)
        (f32x4.convert_i32x4_u (v128.load (i32.const 16))))))`);

var mem32s = new Int32Array(ins.exports.mem.buffer);
var mem32f = new Float32Array(ins.exports.mem.buffer);
var xs = [1, -9, 77987, -34512];

set(mem32s, 4, xs);
ins.exports.convert_s();
assertSame(get(mem32f, 0, 4), xs);

var mem32u = new Uint32Array(ins.exports.mem.buffer);
var ys = xs.map((x) => x>>>0);

set(mem32u, 4, ys);
ins.exports.convert_u();
assertSame(get(mem32f, 0, 4), ys.map(Math.fround));

// Convert floating point to integer with saturating truncation

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "trunc_sat_s")
      (v128.store (i32.const 0)
        (i32x4.trunc_sat_f32x4_s (v128.load (i32.const 16)))))
    (func (export "trunc_sat_u")
      (v128.store (i32.const 0)
        (i32x4.trunc_sat_f32x4_u (v128.load (i32.const 16))))))`);

var mem32s = new Int32Array(ins.exports.mem.buffer);
var mem32u = new Uint32Array(ins.exports.mem.buffer);
var mem32f = new Float32Array(ins.exports.mem.buffer);
var xs = [1.5, -9.5, 7.5e12, -8e13];

set(mem32f, 4, xs);
ins.exports.trunc_sat_s();
assertSame(get(mem32s, 0, 4), [1, -9, 0x7FFFFFFF, -0x80000000]);

var xs = [1.5, -9.5, 7.5e12, 812];
set(mem32f, 4, xs);
ins.exports.trunc_sat_u();
assertSame(get(mem32u, 0, 4), [1, 0, 0xFFFFFFFF, 812]);

var xs = [0, -0, 0x80860000, 0x100000000];
set(mem32f, 4, xs);
ins.exports.trunc_sat_u();
assertSame(get(mem32u, 0, 4), [0, 0, 0x80860000, 0xFFFFFFFF]);

// Loops and blocks.  This should at least test "sync" in the baseline compiler.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $f (param $count i32) (param $v v128) (result v128)
      (local $tmp v128)
      (block $B1
        (loop $L1
          (br_if $B1 (i32.eqz (local.get $count)))
          (local.set $tmp (i32x4.add (local.get $tmp) (local.get $v)))
          (local.set $count (i32.sub (local.get $count) (i32.const 1)))
          (br $L1)))
      (local.get $tmp))
    (func (export "run") (param $count i32)
      (v128.store (i32.const 0)
        (call $f (local.get $count) (v128.load (i32.const 16))))))`);

var mem32 = new Int32Array(ins.exports.mem.buffer);
set(mem32, 4, [1,2,3,4]);
ins.exports.run(7);
assertSame(get(mem32, 0, 4), [7,14,21,28]);

// Lots of parameters, this should trigger stack parameter passing
//
// 10 parameters in memory, we load them and pass them and operate on them.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $f (param $v0 v128) (param $v1 v128) (param $v2 v128) (param $v3 v128) (param $v4 v128)
             (param $v5 v128) (param $v6 v128) (param $v7 v128) (param $v8 v128) (param $v9 v128)
             (result v128)
      (i32x4.add (local.get $v0)
        (i32x4.add (local.get $v1)
          (i32x4.add (local.get $v2)
            (i32x4.add (local.get $v3)
              (i32x4.add (local.get $v4)
                (i32x4.add (local.get $v5)
                  (i32x4.add (local.get $v6)
                    (i32x4.add (local.get $v7)
                      (i32x4.add (local.get $v8) (local.get $v9)))))))))))
    (func (export "run")
      (v128.store (i32.const 0)
        (call $f (v128.load (i32.const ${16*1}))
                 (v128.load (i32.const ${16*2}))
                 (v128.load (i32.const ${16*3}))
                 (v128.load (i32.const ${16*4}))
                 (v128.load (i32.const ${16*5}))
                 (v128.load (i32.const ${16*6}))
                 (v128.load (i32.const ${16*7}))
                 (v128.load (i32.const ${16*8}))
                 (v128.load (i32.const ${16*9}))
                 (v128.load (i32.const ${16*10}))))))`);


var mem32 = new Int32Array(ins.exports.mem.buffer);
var sum = [0, 0, 0, 0];
for ( let i=1; i <= 10; i++ ) {
    let v = [1,2,3,4].map((x) => x*i);
    set(mem32, 4*i, v);
    for ( let j=0; j < 4; j++ )
        sum[j] += v[j];
}

ins.exports.run();

assertSame(get(mem32, 0, 4), sum);

// Globals.
//
// We have a number of different code paths and representations and
// need to test them all.
//
// Cases:
//  - private global, mutable / immutable, initialized from constant or imported immutable global
//  - exported global, mutable / immutable, initialized from constant or imported immutable global
//  - imported global, mutable / immutable
//  - imported global that's re-exported, mutable / immutable

// Global used for initialization below.

var init = (function () {
    var ins = wasmEvalText(`
      (module
        (global (export "init") v128 (v128.const i32x4 9 8 7 6)))`);
    return ins.exports;
})();

for ( let exportspec of ['', '(export "g")'] ) {

    // Private/exported immutable initialized from constant

    let ins1 = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (global $g ${exportspec} v128 (v128.const i32x4 9 8 7 6))
    (func (export "get") (param $dest i32)
      (v128.store (local.get $dest) (global.get $g))))`);

    let mem1 = new Int32Array(ins1.exports.mem.buffer);
    ins1.exports.get(0);
    assertSame(get(mem1, 0, 4), [9, 8, 7, 6]);

    // Private/exported mutable initialized from constant

    let ins2 = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (global $g ${exportspec} (mut v128) (v128.const i32x4 9 8 7 6))
    (func (export "put") (param $val i32)
      (global.set $g (i32x4.splat (local.get $val))))
    (func (export "get") (param $dest i32)
      (v128.store (local.get $dest) (global.get $g))))`);

    let mem2 = new Int32Array(ins2.exports.mem.buffer);
    ins2.exports.get(0);
    assertSame(get(mem2, 0, 4), [9, 8, 7, 6]);
    ins2.exports.put(37);
    ins2.exports.get(0);
    assertSame(get(mem2, 0, 4), [37, 37, 37, 37]);

    // Private/exported immutable initialized from imported immutable global

    let ins3 = wasmEvalText(`
  (module
    (global $init (import "m" "init") v128)
    (memory (export "mem") 1 1)
    (global $g ${exportspec} v128 (global.get $init))
    (func (export "get") (param $dest i32)
      (v128.store (local.get $dest) (global.get $g))))`,
                       {m:init});

    let mem3 = new Int32Array(ins3.exports.mem.buffer);
    ins3.exports.get(0);
    assertSame(get(mem3, 0, 4), [9, 8, 7, 6]);

    // Private/exported mutable initialized from imported immutable global

    let ins4 = wasmEvalText(`
  (module
    (global $init (import "m" "init") v128)
    (memory (export "mem") 1 1)
    (global $g ${exportspec} (mut v128) (global.get $init))
    (func (export "put") (param $val i32)
      (global.set $g (i32x4.splat (local.get $val))))
    (func (export "get") (param $dest i32)
      (v128.store (local.get $dest) (global.get $g))))`,
                       {m:init});

    let mem4 = new Int32Array(ins4.exports.mem.buffer);
    ins4.exports.get(0);
    assertSame(get(mem4, 0, 4), [9, 8, 7, 6]);
    ins4.exports.put(37);
    ins4.exports.get(0);
    assertSame(get(mem4, 0, 4), [37, 37, 37, 37]);

    // Imported private/re-exported immutable

    let ins5 = wasmEvalText(`
  (module
    (global $g ${exportspec} (import "m" "init") v128)
    (memory (export "mem") 1 1)
    (func (export "get") (param $dest i32)
      (v128.store (local.get $dest) (global.get $g))))`,
                       {m:init});

    let mem5 = new Int32Array(ins5.exports.mem.buffer);
    ins5.exports.get(0);
    assertSame(get(mem5, 0, 4), [9, 8, 7, 6]);

    // Imported private/re-exported mutable

    let mutg = (function () {
        var ins = wasmEvalText(`
      (module
        (global (export "mutg") (mut v128) (v128.const i32x4 19 18 17 16)))`);
        return ins.exports;
    })();

    let ins6 = wasmEvalText(`
  (module
    (global $g ${exportspec} (import "m" "mutg") (mut v128))
    (memory (export "mem") 1 1)
    (func (export "put") (param $val i32)
      (global.set $g (i32x4.splat (local.get $val))))
    (func (export "get") (param $dest i32)
      (v128.store (local.get $dest) (global.get $g))))`,
                       {m:mutg});

    let mem6 = new Int32Array(ins6.exports.mem.buffer);
    ins6.exports.get(0);
    assertSame(get(mem6, 0, 4), [19, 18, 17, 16]);
    ins6.exports.put(37);
    ins6.exports.get(0);
    assertSame(get(mem6, 0, 4), [37, 37, 37, 37]);
}

// Imports and exports that pass and return v128

var insworker = wasmEvalText(`
  (module
    (func (export "worker") (param v128) (result v128)
      (i8x16.add (local.get 0) (v128.const i8x16 ${iota(16).join(' ')}))))`);

var insrun = wasmEvalText(`
  (module
    (import "" "worker" (func $worker (param v128) (result v128)))
    (memory (export "mem") 1 1)
    (func (export "run") (param $srcloc i32) (param $destloc i32)
      (v128.store (local.get $destloc)
        (call $worker (v128.load (local.get $srcloc))))))`,
                          {"":insworker.exports});

var mem = new Uint8Array(insrun.exports.mem.buffer);
var xs = iota(16).map((x) => x+5);
set(mem, 0, xs);
insrun.exports.run(0, 16);
assertSame(get(mem, 16, 16), xs.map((x,i) => x+i))

// Make sure JS<->wasm call guards are sensible.

// Calling from JS to export that accepts v128.
assertErrorMessage(() => insworker.exports.worker(),
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

// Calling from wasm with v128 to import that comes from JS.  The instantiation
// will succeed even if the param type of the import is v128 (see "create a host
// function" in the Wasm JSAPI spec), it is the act of invoking it that checks
// that verboten types are not used (see "run a host function", ibid.).
var badImporter = wasmEvalText(`
  (module
    (import "" "worker" (func $worker (param v128) (result v128)))
    (func (export "run")
      (drop (call $worker (v128.const i32x4 0 1 2 3)))))`,
             {"":{worker: function(a) { return a; }}});

assertErrorMessage(() => badImporter.exports.run(),
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

// Imports and exports that pass and return v128 as stack (not register) args.

var exportWithStackArgs = wasmEvalText(`
  (module
    (func (export "worker") (param v128) (param v128) (param v128) (param v128)
                            (param v128) (param v128) (param v128) (param v128)
                            (param v128) (param v128) (param v128) (param v128)
                            (param v128) (param v128)
           (result v128 v128)
      (i8x16.add (local.get 3) (local.get 12))
      (local.get 7)))`);

var importWithStackArgs = wasmEvalText(`
  (module
    (type $t1 (func (param v128) (param v128) (param v128) (param v128)
                    (param v128) (param v128) (param v128) (param v128)
                    (param v128) (param v128) (param v128) (param v128)
                    (param v128) (param v128)
                    (result v128 v128)))
    (import "" "worker" (func $worker (type $t1)))
    (memory (export "mem") 1 1)
    (table funcref (elem $worker))
    (func (export "run")
      (i32.const 16)
      (call_indirect (type $t1) (v128.const i32x4 1 1 1 1) (v128.const i32x4 2 2 2 2) (v128.const i32x4 3 3 3 3)
                    (v128.const i32x4 4 4 4 4) (v128.const i32x4 5 5 5 5) (v128.const i32x4 6 6 6 6)
                    (v128.const i32x4 7 7 7 7) (v128.const i32x4 8 8 8 8) (v128.const i32x4 9 9 9 9)
                    (v128.const i32x4 10 10 10 10) (v128.const i32x4 11 11 11 11) (v128.const i32x4 12 12 12 12)
                    (v128.const i32x4 13 13 13 13) (v128.const i32x4 14 14 14 14)
           (i32.const 0))
      drop
      v128.store
      (i32.const 0)
      (call $worker (v128.const i32x4 1 1 1 1) (v128.const i32x4 2 2 2 2) (v128.const i32x4 3 3 3 3)
                    (v128.const i32x4 4 4 4 4) (v128.const i32x4 5 5 5 5) (v128.const i32x4 6 6 6 6)
                    (v128.const i32x4 7 7 7 7) (v128.const i32x4 8 8 8 8) (v128.const i32x4 9 9 9 9)
                    (v128.const i32x4 10 10 10 10) (v128.const i32x4 11 11 11 11) (v128.const i32x4 12 12 12 12)
                    (v128.const i32x4 13 13 13 13) (v128.const i32x4 14 14 14 14))
      drop
      v128.store))`,
                                       {"": exportWithStackArgs.exports});

var mem = new Int32Array(importWithStackArgs.exports.mem.buffer);
importWithStackArgs.exports.run();
assertSame(get(mem, 0, 4), [17, 17, 17, 17]);
assertSame(get(mem, 4, 4), [17, 17, 17, 17]);

// Imports and exports of v128 globals

var insexporter = wasmEvalText(`
  (module
    (global (export "myglobal") (mut v128) (v128.const i8x16 ${iota(16).join(' ')})))`);

var insimporter = wasmEvalText(`
  (module
    (import "m" "myglobal" (global $g (mut v128)))
    (memory (export "mem") 1 1)
    (func (export "run") (param $dest i32)
      (v128.store (local.get $dest) (global.get $g))))`,
                               {m:insexporter.exports});

var mem = new Uint8Array(insimporter.exports.mem.buffer);
insimporter.exports.run(16);
assertSame(get(mem, 16, 16), iota(16));

// Guards on accessing v128 globals from JS

assertErrorMessage(() => insexporter.exports.myglobal.value = 0,
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

assertErrorMessage(function () { let v = insexporter.exports.myglobal.value },
                   TypeError,
                   /cannot pass.*v128.*to or from JS/);

// Multi-value cases + v128 parameters to if, block, loop

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func $mvreturn (result v128 v128 v128)
      (v128.load (i32.const 16))
      (v128.load (i32.const 0))
      (v128.load (i32.const 32)))
    (func (export "runreturn")
      i32.const 48
      (call $mvreturn)
      i32x4.sub ;; [-20, -20, -20, -20]
      i32x4.sub ;; [31, 32, 33, 34]
      v128.store)
    (func (export "runif") (param $cond i32)
      i32.const 48
      (v128.load (i32.const 0))
      (v128.load (i32.const 16))
      (if (param v128) (param v128) (result v128 v128)
          (local.get $cond)
          (then i32x4.add
                (v128.load (i32.const 32)))
          (else i32x4.sub
                (v128.load (i32.const 0))))
      i32x4.add
      v128.store)
    (func (export "runblock")
      i32.const 48
      (v128.load (i32.const 0))
      (v128.load (i32.const 16))
      (block (param v128 v128) (result v128 v128)
          i32x4.add
          (v128.load (i32.const 32)))
      i32x4.add
      v128.store)
    (func (export "runloop") (param $count i32)
      i32.const 48
      (v128.load (i32.const 0))
      (v128.load (i32.const 16))
      (block $B (param v128 v128) (result v128 v128)
        (loop $L (param v128 v128) (result v128 v128)
          i32x4.add
          (v128.load (i32.const 32))
          (local.set $count (i32.sub (local.get $count) (i32.const 1)))
          (br_if $B (i32.eqz (local.get $count)))
          (br $L)))
      i32x4.add
      v128.store))`);

var mem = new Int32Array(ins.exports.mem.buffer);
set(mem, 0, [1, 2, 3, 4]);
set(mem, 4, [11, 12, 13, 14]);
set(mem, 8, [21, 22, 23, 24]);

// Multi-value returns

ins.exports.runreturn();
assertSame(get(mem, 12, 4), [31, 32, 33, 34]);

// Multi-parameters to and multi-returns from "if"

// This should be vector@0 + vector@16 + vector@32
ins.exports.runif(1);
assertSame(get(mem, 12, 4),
           [33, 36, 39, 42]);

// This should be vector@0 - vector@16 + vector@0
ins.exports.runif(0);
assertSame(get(mem, 12, 4),
           [-9, -8, -7, -6]);

// This should be vector@0 + vector@16 + vector@32
ins.exports.runblock();
assertSame(get(mem, 12, 4),
           [33, 36, 39, 42]);

// This should be vector@0 + vector@16 + N * vector@32 where
// N is the parameter to runloop.
ins.exports.runloop(3);
assertSame(get(mem, 12, 4),
           [12+3*21, 14+3*22, 16+3*23, 18+3*24]);
