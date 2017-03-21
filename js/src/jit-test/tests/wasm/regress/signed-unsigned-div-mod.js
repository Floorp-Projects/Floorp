setJitCompilerOption('wasm.test-mode', 1);

// The result should be -1 because it is (i32.rem_s -1 10000) and the spec
// stipulates "result has the sign of the dividend".  This test uncovers a bug
// in SpiderMonkey wherein the shape of the lhs (it looks like an unsigned
// value) causes an unsigned modulo to be emitted.

assertEq(wasmEvalText(
    `(module
      (func (result i32)
       (i32.const -1)
       (i32.const 0)
       i32.shr_u
       (i32.const 10000)
       i32.rem_s)
      (export "f" 0))`).exports.f(), -1);

// Ditto for int64

assertEqI64(wasmEvalText(
    `(module
      (func (result i64)
       (i64.const -1)
       (i64.const 0)
       i64.shr_u
       (i64.const 10000)
       i64.rem_s)
      (export "f" 0))`).exports.f(), {low:-1, high:-1});

// Despite the signed shift this is 0x80000000 % 10000 (rem_u)
// and the result is positive.

assertEq(wasmEvalText(
    `(module
      (func (result i32)
       (i32.const -1)
       (i32.const 0)
       i32.shl
       (i32.const 10000)
       i32.rem_u)
      (export "f" 0))`).exports.f(), 7295);

// 0x80000000 is really -0x80000000 so the result of signed division shall be
// negative.

assertEq(wasmEvalText(
    `(module
      (func (result i32)
       (i32.const 0x80000000)
       (i32.const 0)
       i32.shr_u
       (i32.const 10000)
       i32.div_s)
      (export "f" 0))`).exports.f(), -214748);

assertEq(wasmEvalText(
    `(module
      (func (result i32)
       (i32.const 0x80000000)
       (i32.const 0)
       i32.shr_u
       (i32.const -10000)
       i32.div_s)
      (export "f" 0))`).exports.f(), 214748);

// And the result of unsigned division shall be positive.

assertEq(wasmEvalText(
    `(module
      (func (result i32)
       (i32.const 0x80000000)
       (i32.const 0)
       i32.shr_u
       (i32.const 10000)
       i32.div_u)
      (export "f" 0))`).exports.f(), 214748);

