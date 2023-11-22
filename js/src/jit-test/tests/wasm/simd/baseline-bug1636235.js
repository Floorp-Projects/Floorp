// |jit-test| skip-if: !wasmSimdEnabled()

// Bug 1636235: assorted corner case baseline SIMD bugs.

function get(arr, loc, len) {
    let res = [];
    for ( let i=0; i < len; i++ ) {
        res.push(arr[loc+i]);
    }
    return res;
}

// Pass v128 along a control flow edge in br_table

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "run") (param $k i32)
      (v128.store (i32.const 0) (call $f (local.get $k))))
    (func $f (param $k i32) (result v128)
      (block $B2 (result v128)
        (block $B1 (result v128)
          (v128.const i32x4 1 2 3 4)
          (br_table $B1 $B2 (local.get $k)))
        (drop)
        (v128.const i32x4 5 6 7 8))))`);

var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run(0);
assertDeepEq(get(mem, 0, 4), [5, 6, 7, 8]);

ins.exports.run(1);
assertDeepEq(get(mem, 0, 4), [1, 2, 3, 4]);

// Materialize a ConstV128 off the value stack in popStackResults (also: check
// that br passing v128 values works as it should).

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)

    (func (export "run") (param $k i32)
      (local $t0 v128) (local $t1 v128) (local $t2 v128)
      (call $f (local.get $k))
      (local.set $t2)
      (local.set $t1)
      (local.set $t0)
      (v128.store (i32.const 32) (local.get $t2))
      (v128.store (i32.const 16) (local.get $t1))
      (v128.store (i32.const 0) (local.get $t0)))

    (func $f (param $k i32) (result v128 v128 v128)
      (block $B2 (result v128 v128 v128)
        (if (local.get $k)
            (then 
              (br $B2 (v128.const i32x4 5 6 7 8)
                    (v128.const i32x4 9 10 11 12)
                    (v128.const i32x4 13 14 15 16)))
            (else
              (br $B2 (v128.const i32x4 -5 -6 -7 -8)
                    (v128.const i32x4 -9 -10 -11 -12)
                    (v128.const i32x4 -13 -14 -15 -16))))
        (unreachable))))`);

var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run(0);
assertDeepEq(get(mem, 0, 4), [-5, -6, -7, -8]);
assertDeepEq(get(mem, 4, 4), [-9, -10, -11, -12]);
assertDeepEq(get(mem, 8, 4), [-13, -14, -15, -16]);

ins.exports.run(1);
assertDeepEq(get(mem, 0, 4), [5, 6, 7, 8]);
assertDeepEq(get(mem, 4, 4), [9, 10, 11, 12]);
assertDeepEq(get(mem, 8, 4), [13, 14, 15, 16]);

// Check that br_if passing v128 values works as it should.

var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)

    (func (export "run") (param $k i32)
      (local $t0 v128) (local $t1 v128) (local $t2 v128)
      (call $f (local.get $k))
      (local.set $t2)
      (local.set $t1)
      (local.set $t0)
      (v128.store (i32.const 32) (local.get $t2))
      (v128.store (i32.const 16) (local.get $t1))
      (v128.store (i32.const 0) (local.get $t0)))

    (func $f (param $k i32) (result v128 v128 v128)
      (block $B2 (result v128 v128 v128)
        (v128.const i32x4 5 6 7 8)
        (v128.const i32x4 9 10 11 12)
        (v128.const i32x4 13 14 15 16)
        (br_if $B2 (local.get $k))
        drop drop drop
        (v128.const i32x4 -5 -6 -7 -8)
        (v128.const i32x4 -9 -10 -11 -12)
        (v128.const i32x4 -13 -14 -15 -16))))`);

var mem = new Int32Array(ins.exports.mem.buffer);
ins.exports.run(0);
assertDeepEq(get(mem, 0, 4), [-5, -6, -7, -8]);
assertDeepEq(get(mem, 4, 4), [-9, -10, -11, -12]);
assertDeepEq(get(mem, 8, 4), [-13, -14, -15, -16]);

ins.exports.run(1);
assertDeepEq(get(mem, 0, 4), [5, 6, 7, 8]);
assertDeepEq(get(mem, 4, 4), [9, 10, 11, 12]);
assertDeepEq(get(mem, 8, 4), [13, 14, 15, 16]);

