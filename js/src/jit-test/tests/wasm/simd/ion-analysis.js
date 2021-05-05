// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !this.wasmSimdAnalysis

// White-box tests for SIMD optimizations.  These are sensitive to internal
// details of the front-end and lowering logic, which is partly platform-dependent.
//
// In DEBUG builds, the testing function wasmSimdAnalysis() returns a string
// describing the last decision made by the SIMD lowering code: to perform an
// optimized lowering or the default byte shuffle+blend for i8x16.shuffle; to
// shift by a constant or a variable for the various shifts; and so on.
//
// We test that the expected transformation applies, and that the machine code
// generates the expected result.

var isArm64 = getBuildConfiguration().arm64;

// 32-bit permutation that is not a rotation.
let perm32x4_pattern = [4, 5, 6, 7, 12, 13, 14, 15, 8, 9, 10, 11, 0, 1, 2, 3];

// Operands the same, dword permutation
{
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${perm32x4_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 32x4");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), perm32x4_pattern);
}

// Right operand ignored, dword permutation
{
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${perm32x4_pattern.join(' ')} (local.get 0) (local.get 1))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 32x4");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    set(mem, 32, iota(16).map(x => x+16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), perm32x4_pattern);
}

// Left operand ignored, dword permutation
{
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${perm32x4_pattern.map(x => x+16).join(' ')} (local.get 0) (local.get 1))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 32x4");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16).map(x => x+16));
    set(mem, 32, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), perm32x4_pattern);
}

// Operands the same, word permutation on both sides of the qword divide, with a qword swap
{
    let perm16x8_pattern = [12, 13, 14, 15, 10, 11, 8, 9,
                             6,  7,  4,  5,  2,  3, 0, 1];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${perm16x8_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 16x8 swap high low");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), perm16x8_pattern);
}

// Operands the same, word permutation on both sides of the qword divide, no qword swap
{
    let perm16x8_pattern = [ 6,  7,  4,  5,  2,  3, 0, 1,
                            12, 13, 14, 15, 10, 11, 8, 9];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${perm16x8_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 16x8 high low");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), perm16x8_pattern);
}

// Operands the same, word permutation on low side of the qword divide, no qword swap
{
    let perm16x8_pattern = [ 6, 7,  4,  5,  2,  3,  0,  1,
                             8, 9, 10, 11, 12, 13, 14, 15];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${perm16x8_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 16x8 low");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), perm16x8_pattern);
}

// Operands the same, word permutation on high side of the qword divide, no qword swap
{
    let perm16x8_pattern = [ 0,  1,  2,  3,  4,  5, 6, 7,
                            12, 13, 14, 15, 10, 11, 8, 9];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${perm16x8_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 16x8 high");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), perm16x8_pattern);
}

// Same operands, byte rotate
{
    // 8-bit permutation that is a rotation
    let rot8x16_pattern = [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${rot8x16_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> rotate-right 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), rot8x16_pattern);
}

// Operands the same, random jumble => byte permutation
{
    // 8-bit permutation that is not a rotation
    let perm8x16_pattern = [5, 7, 6, 8, 9, 10, 11, 4, 13, 14, 15, 0, 1, 2, 3, 12];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${perm8x16_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), perm8x16_pattern);
}

// Operands differ, both accessed, rhs is constant zero, left-shift pattern
{
    // 8-bit shift with zeroes shifted in at the right end
    let shift8x16_pattern = [16, 16, 16, 16, 16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${shift8x16_pattern.join(' ')} (local.get 0) (v128.const i32x4 0 0 0 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> shift-left 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), shift8x16_pattern.map(x => x >= 16 ? 0 : x));
}

// The same as above but the constant is lhs.
{
    // 8-bit shift with zeroes shifted in at the right end
    let shift8x16_pattern = [16, 16, 16, 16, 16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10].map(x => x ^ 16);
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${shift8x16_pattern.join(' ')} (v128.const i32x4 0 0 0 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> shift-left 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), shift8x16_pattern.map(x => x < 16 ? 0 : x - 16));
}

// Operands differ, both accessed, rhs is constant zero, left-shift pattern that
// does not start properly.
{
    // 8-bit shift with zeroes shifted in at the right end
    let shift8x16_pattern = [16, 16, 16, 16, 16, 16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${shift8x16_pattern.join(' ')} (local.get 0) (v128.const i32x4 0 0 0 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> shuffle+blend 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), shift8x16_pattern.map(x => x >= 16 ? 0 : x));
}

// Operands differ, both accessed, rhs is constant zero, right-shift pattern
{
    // 8-bit shift with zeroes shifted in at the right end
    let shift8x16_pattern = [6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 20, 20, 20, 20, 20, 20];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${shift8x16_pattern.join(' ')} (local.get 0) (v128.const i32x4 0 0 0 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> shift-right 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), shift8x16_pattern.map(x => x >= 16 ? 0 : x));
}

// Operands differ, both accessed, rhs is constant zero, right-shift pattern
// that does not end properly.
{
    // 8-bit shift with zeroes shifted in at the right end
    let shift8x16_pattern = [6, 7, 8, 9, 10, 11, 12, 13, 14, 20, 20, 20, 20, 20, 20, 20];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${shift8x16_pattern.join(' ')} (local.get 0) (v128.const i32x4 0 0 0 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> shuffle+blend 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), shift8x16_pattern.map(x => x >= 16 ? 0 : x));
}

// Operands differ and are variable, both accessed, (lhs ++ rhs) >> k
{
    let concat8x16_pattern = [27, 28, 29, 30, 31, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${concat8x16_pattern.join(' ')} (local.get 0) (local.get 1))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> concat+shift-right 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    set(mem, 32, iota(16).map(k => k+16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), concat8x16_pattern);
}

// Operands differ and are variable, both accessed, (rhs ++ lhs) >> k
{
    let concat8x16_pattern = [11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${concat8x16_pattern.join(' ')} (local.get 0) (local.get 1))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> concat+shift-right 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    set(mem, 32, iota(16).map(k => k+16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), concat8x16_pattern);
}

// Operands differ, both accessed, but inputs stay in their lanes => byte blend
{
    let blend8x16_pattern = iota(16).map(x => (x % 3 == 0) ? x + 16 : x);
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${blend8x16_pattern.join(' ')} (local.get 0) (local.get 1))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> blend 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    let lhs = iota(16);
    let rhs = iota(16).map(x => x+16);
    set(mem, 16, lhs);
    set(mem, 32, rhs);
    ins.exports.run();
    assertSame(get(mem, 0, 16), blend8x16_pattern);
}

// Operands differ, both accessed, but inputs stay in their lanes => word blend
{
    let blend16x8_pattern = iota(16).map(x => (x & 2) ? x + 16 : x);
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${blend16x8_pattern.join(' ')} (local.get 0) (local.get 1))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> blend 16x8");

    let mem = new Int8Array(ins.exports.mem.buffer);
    let lhs = iota(16);
    let rhs = iota(16).map(x => x+16);
    set(mem, 16, lhs);
    set(mem, 32, rhs);
    ins.exports.run();
    assertSame(get(mem, 0, 16), blend16x8_pattern);
}

// Interleave i32x4s
for ( let [lhs, rhs, expected] of
      [[[0, 1], [4, 5], "shuffle -> interleave-low 32x4"],
       [[2, 3], [6, 7], "shuffle -> interleave-high 32x4"]] ) {
    for (let swap of [false, true]) {
        if (swap)
            [lhs, rhs] = [rhs, lhs];
        let interleave_pattern = i32ToI8(interleave(lhs, rhs));
        let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${interleave_pattern.join(' ')} (local.get 0) (local.get 1))))`);

        assertEq(wasmSimdAnalysis(), expected);

        let mem = new Int8Array(ins.exports.mem.buffer);
        let lhsval = iota(16);
        let rhsval = iota(16).map(x => x+16);
        set(mem, 16, lhsval);
        set(mem, 32, rhsval);
        ins.exports.run();
        assertSame(get(mem, 0, 16), interleave_pattern);
    }
}

// Interleave i64x2s
for ( let [lhs, rhs, expected] of
  [[[0], [2], "shuffle -> interleave-low 64x2"],
   [[1], [3], "shuffle -> interleave-high 64x2"]] ) {
    for (let swap of [false, true]) {
        if (swap)
            [lhs, rhs] = [rhs, lhs];
        let interleave_pattern = i64ToI2(interleave(lhs, rhs));
        let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${interleave_pattern.join(' ')} (local.get 0) (local.get 1))))`);

        assertEq(wasmSimdAnalysis(), expected);

        let mem = new Int8Array(ins.exports.mem.buffer);
        let lhsval = iota(16);
        let rhsval = iota(16).map(x => x+16);
        set(mem, 16, lhsval);
        set(mem, 32, rhsval);
        ins.exports.run();
        assertSame(get(mem, 0, 16), interleave_pattern);
    }
}

// Interleave i16x8s
for ( let [lhs, rhs, expected] of
      [[[0, 1, 2, 3], [8, 9, 10, 11], "shuffle -> interleave-low 16x8"],
       [[4, 5, 6, 7], [12, 13, 14, 15], "shuffle -> interleave-high 16x8"]] ) {
    for (let swap of [false, true]) {
        if (swap)
            [lhs, rhs] = [rhs, lhs];
        let interleave_pattern = i16ToI8(interleave(lhs, rhs));
        let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${interleave_pattern.join(' ')} (local.get 0) (local.get 1))))`);

        assertEq(wasmSimdAnalysis(), expected);

        let mem = new Int8Array(ins.exports.mem.buffer);
        let lhsval = iota(16);
        let rhsval = iota(16).map(x => x+16);
        set(mem, 16, lhsval);
        set(mem, 32, rhsval);
        ins.exports.run();
        assertSame(get(mem, 0, 16), interleave_pattern);
    }
}

// Interleave i8x16s
for ( let [lhs, rhs, expected] of
      [[[0, 1, 2, 3, 4, 5, 6, 7],      [16, 17, 18, 19, 20, 21, 22, 23], "shuffle -> interleave-low 8x16"],
       [[8, 9, 10, 11, 12, 13, 14, 15],[24, 25, 26, 27, 28, 29, 30, 31], "shuffle -> interleave-high 8x16"]] ) {
    for (let swap of [false, true]) {
        if (swap)
            [lhs, rhs] = [rhs, lhs];
        let interleave_pattern = interleave(lhs, rhs);
        let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${interleave_pattern.join(' ')} (local.get 0) (local.get 1))))`);

        assertEq(wasmSimdAnalysis(), expected);

        let mem = new Int8Array(ins.exports.mem.buffer);
        let lhsval = iota(16);
        let rhsval = iota(16).map(x => x+16);
        set(mem, 16, lhsval);
        set(mem, 32, rhsval);
        ins.exports.run();
        assertSame(get(mem, 0, 16), interleave_pattern);
    }
}

// Operands differ, both accessed, random jumble => byte shuffle+blend
{
    let blend_perm8x16_pattern = [5, 23, 6, 24, 9, 10, 11, 7, 7, 14, 15, 19, 1, 2, 3, 12];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${blend_perm8x16_pattern.join(' ')} (local.get 0) (local.get 1))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> shuffle+blend 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    let lhs = iota(16).map(x => x+16);
    let rhs = iota(16);
    set(mem, 16, lhs);
    set(mem, 32, rhs);
    ins.exports.run();
    assertSame(get(mem, 0, 16),
               blend_perm8x16_pattern.map(x => x < 16 ? lhs[x] : rhs[x-16]));
}

// No-op, ignoring right operand, should turn into a move.
{
    let nop8x16_pattern = iota(16);
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${nop8x16_pattern.join(' ')} (local.get 0) (local.get 1))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> move");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    set(mem, 32, iota(16).map(x => x+16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), nop8x16_pattern);
}

// No-op, ignoring left operand, should turn into a move.
{
    let nop8x16_pattern = iota(16).map(x => x+16);
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
  (func $f (param v128) (param v128) (result v128)
    (i8x16.shuffle ${nop8x16_pattern.join(' ')} (local.get 0) (local.get 1))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> move");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    set(mem, 32, iota(16).map(x => x+16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), nop8x16_pattern);
}

// Broadcast byte
for ( let byte of [3, 11, 8, 2] ) {
    let broadcast8x16_pattern = iota(16).map(_ => byte);
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${broadcast8x16_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> broadcast 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), broadcast8x16_pattern);
}

// Broadcast word from high quadword
{
    let broadcast16x8_pattern = [10, 11, 10, 11, 10, 11, 10, 11, 10, 11, 10, 11, 10, 11, 10, 11];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${broadcast16x8_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> broadcast 16x8");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), broadcast16x8_pattern);
}

// Broadcast word from low quadword
{
    let broadcast16x8_pattern = [4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${broadcast16x8_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> broadcast 16x8");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), broadcast16x8_pattern);
}

// Broadcast dword from low quadword should turn into a dword permute
{
    let broadcast32x4_pattern = [4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7];
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${broadcast32x4_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 32x4");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), broadcast32x4_pattern);
}

// Broadcast high qword should turn into a dword permute
{
    let broadcast64x2_pattern = [8, 9, 10, 11, 12, 13, 14, 15, 8, 9, 10, 11, 12, 13, 14, 15]
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${broadcast64x2_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 32x4");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), broadcast64x2_pattern);
}

// Byte reversal should be a byte permute
{
    let rev8x16_pattern = iota(16).reverse();
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${rev8x16_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 8x16");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), rev8x16_pattern);
}

// Word reversal should be a word permute
{
    let rev16x8_pattern = i16ToI8(iota(8).reverse());
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${rev16x8_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 16x8 swap high low");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), rev16x8_pattern);
}

// Dword reversal should be a dword permute
{
    let rev32x4_pattern = i32ToI8([3, 2, 1, 0]);
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${rev32x4_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 32x4");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), rev32x4_pattern);
}

// Qword reversal should be a dword permute
{
    let rev64x2_pattern = i32ToI8([2, 3, 0, 1]);
    let ins = wasmCompile(`
(module
  (memory (export "mem") 1 1)
  (func (export "run")
    (v128.store (i32.const 0) (call $f (v128.load (i32.const 16)))))
  (func $f (param v128) (result v128)
    (i8x16.shuffle ${rev64x2_pattern.join(' ')} (local.get 0) (local.get 0))))`);

    assertEq(wasmSimdAnalysis(), "shuffle -> permute 32x4");

    let mem = new Int8Array(ins.exports.mem.buffer);
    set(mem, 16, iota(16));
    ins.exports.run();
    assertSame(get(mem, 0, 16), rev64x2_pattern);
}

// In the case of shifts, we have separate tests that constant shifts work
// correctly, so no such testing is done here.

for ( let lanes of ['i8x16', 'i16x8', 'i32x4', 'i64x2'] ) {
    for ( let shift of ['shl', 'shr_s', 'shr_u'] ) {
        for ( let [count, result] of [['(i32.const 5)', /shift -> constant shift/],
                                      ['(local.get 1)', /shift -> variable(?: scalarized)? shift/]] ) {
            wasmCompile(`(module (func (param v128) (param i32) (result v128) (${lanes}.${shift} (local.get 0) ${count})))`);
            assertEq(wasmSimdAnalysis().match(result).length, 1);
        }
    }
}

// Constant folding scalar->simd.  There are functional tests for all these in
// ad-hack.js so here we only check that the transformation is triggered.

for ( let [ty128, ty] of [['i8x16', 'i32'], ['i16x8', 'i32'], ['i32x4', 'i32'],
                          ['i64x2', 'i64'], ['f32x4', 'f32'], ['f64x2', 'f64']] )
{
    wasmCompile(`(module (func (result v128) (${ty128}.splat (${ty}.const 37))))`);
    assertEq(wasmSimdAnalysis(), "scalar-to-simd128 -> constant folded");
}

// Ditto simd->scalar.

for ( let [ty128, suffix] of [['i8x16', '_s'], ['i8x16', '_u'], ['i16x8','_s'], ['i16x8','_u'], ['i32x4', '']] ) {
    for ( let op of ['any_true', 'all_true', 'bitmask', `extract_lane${suffix} 0`] ) {
        let operation = op == 'any_true' ? 'v128.any_true' : `${ty128}.${op}`;
        wasmCompile(`(module (func (result i32) (${operation} (v128.const i64x2 0 0))))`);
        assertEq(wasmSimdAnalysis(), "simd128-to-scalar -> constant folded");
    }
}

for ( let ty128 of ['f32x4','f64x2','i64x2'] ) {
    wasmCompile(`(module (func (result ${ty128.match(/(...)x.*/)[1]}) (${ty128}.extract_lane 0 (v128.const i64x2 0 0))))`);
    assertEq(wasmSimdAnalysis(), "simd128-to-scalar -> constant folded");
}

// Optimizing all_true, any_true, and bitmask that are used for control flow, also when negated.

for ( let [ty128,size] of [['i8x16',1], ['i16x8',2], ['i32x4',4]] ) {
    let all = iota(16/size).map(n => n*n);
    let some = iota(16/size).map(n => n*(n % 3));
    let none = iota(16/size).map(n => 0);
    let inputs = [all, some, none];
    let ops = { all_true: allTrue, any_true: anyTrue, bitmask };

    for ( let op of ['any_true', 'all_true', 'bitmask'] ) {
        let folded = op != 'bitmask' || (size == 2 && !isArm64);
        let operation = op == 'any_true' ? 'v128.any_true' : `${ty128}.${op}`;
        let positive =
            wasmCompile(
                `(module
                   (memory (export "mem") 1 1)
                   (func $f (param v128) (result i32)
                       (if (result i32) (${operation} (local.get 0))
                           (i32.const 42)
                           (i32.const 37)))
                   (func (export "run") (result i32)
                     (call $f (v128.load (i32.const 16)))))`);
        assertEq(wasmSimdAnalysis(), folded ? "simd128-to-scalar-and-branch -> folded" : "none");

        let negative =
            wasmCompile(
                `(module
                   (memory (export "mem") 1 1)
                   (func $f (param v128) (result i32)
                       (if (result i32) (i32.eqz (${operation} (local.get 0)))
                           (i32.const 42)
                           (i32.const 37)))
                   (func (export "run") (result i32)
                     (call $f (v128.load (i32.const 16)))))`);
        assertEq(wasmSimdAnalysis(), folded ? "simd128-to-scalar-and-branch -> folded" : "none");

        for ( let inp of inputs ) {
            let mem = new this[`Int${8*size}Array`](positive.exports.mem.buffer);
            set(mem, 16/size, inp);
            assertEq(positive.exports.run(), ops[op](inp) ? 42 : 37);

            mem = new this[`Int${8*size}Array`](negative.exports.mem.buffer);
            set(mem, 16/size, inp);
            assertEq(negative.exports.run(), ops[op](inp) ? 37 : 42);
        }
    }
}

// Constant folding

{
    // Swizzle-with-constant rewritten as shuffle, and then further optimized
    // into a dword permute.  Correctness is tested in ad-hack.js.
    wasmCompile(`
(module (func (param v128) (result v128)
  (i8x16.swizzle (local.get 0) (v128.const i8x16 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11))))
`);
    assertEq(wasmSimdAnalysis(), "shuffle -> permute 32x4");
}

// Library

function wasmCompile(text) {
    return new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(text)))
}

function get(arr, loc, len) {
    let res = [];
    for ( let i=0; i < len; i++ ) {
        res.push(arr[loc+i]);
    }
    return res;
}

function set(arr, loc, vals) {
    for ( let i=0; i < vals.length; i++ ) {
        arr[loc+i] = vals[i];
    }
}

function assertSame(got, expected) {
    assertEq(got.length, expected.length);
    for ( let i=0; i < got.length; i++ ) {
        assertEq(got[i], expected[i]);
    }
}
function iota(len) {
    let xs = [];
    for ( let i=0 ; i < len ; i++ )
        xs.push(i);
    return xs;
}

function interleave(xs, ys) {
    let res = [];
    for ( let i=0 ; i < xs.length; i++ ) {
        res.push(xs[i]);
        res.push(ys[i]);
    }
    return res;
}

function i32ToI8(xs) {
    return xs.map(x => [x*4, x*4+1, x*4+2, x*4+3]).flat();
}

function i64ToI2(xs) {
  return xs.map(x => [x*8, x*8+1, x*8+2, x*8+3,
                      x*8+4, x*8+5, x*8+6, x*8+7]).flat();
}

function i16ToI8(xs) {
    return xs.map(x => [x*2, x*2+1]).flat();
}

function allTrue(xs) {
    return xs.every(v => v != 0);
}

function anyTrue(xs) {
    return xs.some(v => v != 0);
}

function bitmask(xs) {
    let shift = 128/xs.length - 1;
    let res = 0;
    let k = 0;
    xs.forEach(v => { res |= ((v >>> shift) & 1) << k; k++; });
    return res;
}
