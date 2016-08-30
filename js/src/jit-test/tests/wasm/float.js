// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

assertEq(wasmEvalText('(module (func (result f32) (f32.const -1)) (export "" 0))')(), -1);
assertEq(wasmEvalText('(module (func (result f32) (f32.const 1)) (export "" 0))')(), 1);
assertEq(wasmEvalText('(module (func (result f64) (f64.const -2)) (export "" 0))')(), -2);
assertEq(wasmEvalText('(module (func (result f64) (f64.const 2)) (export "" 0))')(), 2);
assertEq(wasmEvalText('(module (func (result f64) (f64.const 4294967296)) (export "" 0))')(), 4294967296);
assertEq(wasmEvalText('(module (func (result f32) (f32.const 1.5)) (export "" 0))')(), 1.5);
assertEq(wasmEvalText('(module (func (result f64) (f64.const 2.5)) (export "" 0))')(), 2.5);
assertEq(wasmEvalText('(module (func (result f64) (f64.const 10e2)) (export "" 0))')(), 10e2);
assertEq(wasmEvalText('(module (func (result f32) (f32.const 10e2)) (export "" 0))')(), 10e2);
assertEq(wasmEvalText('(module (func (result f64) (f64.const -0x8000000000000000)) (export "" 0))')(), -0x8000000000000000);
assertEq(wasmEvalText('(module (func (result f64) (f64.const -9223372036854775808)) (export "" 0))')(), -9223372036854775808);
assertEq(wasmEvalText('(module (func (result f64) (f64.const 1797693134862315708145274e284)) (export "" 0))')(), 1797693134862315708145274e284);

function testUnary(type, opcode, op, expect) {
  assertEq(wasmEvalText('(module (func (param ' + type + ') (result ' + type + ') (' + type + '.' + opcode + ' (get_local 0))) (export "" 0))')(op), expect);
}

function testBinary(type, opcode, lhs, rhs, expect) {
  assertEq(wasmEvalText('(module (func (param ' + type + ') (param ' + type + ') (result ' + type + ') (' + type + '.' + opcode + ' (get_local 0) (get_local 1))) (export "" 0))')(lhs, rhs), expect);
}

function testComparison(type, opcode, lhs, rhs, expect) {
  assertEq(wasmEvalText('(module (func (param ' + type + ') (param ' + type + ') (result i32) (' + type + '.' + opcode + ' (get_local 0) (get_local 1))) (export "" 0))')(lhs, rhs), expect);
}

testUnary('f32', 'abs', -40, 40);
testUnary('f32', 'neg', 40, -40);
testUnary('f32', 'floor', 40.9, 40);
testUnary('f32', 'ceil', 40.1, 41);
testUnary('f32', 'nearest', -41.5, -42);
testUnary('f32', 'trunc', -41.5, -41);
testUnary('f32', 'sqrt', 40, 6.324555397033691);

testBinary('f32', 'add', 40, 2, 42);
testBinary('f32', 'sub', 40, 2, 38);
testBinary('f32', 'mul', 40, 2, 80);
testBinary('f32', 'div', 40, 3, 13.333333015441895);
testBinary('f32', 'min', 40, 2, 2);
testBinary('f32', 'max', 40, 2, 40);
testBinary('f32', 'copysign', 40, -2, -40);

testComparison('f32', 'eq', 40, 40, 1);
testComparison('f32', 'ne', 40, 40, 0);
testComparison('f32', 'lt', 40, 40, 0);
testComparison('f32', 'le', 40, 40, 1);
testComparison('f32', 'gt', 40, 40, 0);
testComparison('f32', 'ge', 40, 40, 1);

testUnary('f64', 'abs', -40, 40);
testUnary('f64', 'neg', 40, -40);
testUnary('f64', 'floor', 40.9, 40);
testUnary('f64', 'ceil', 40.1, 41);
testUnary('f64', 'nearest', -41.5, -42);
testUnary('f64', 'trunc', -41.5, -41);
testUnary('f64', 'sqrt', 40, 6.324555320336759);

testBinary('f64', 'add', 40, 2, 42);
testBinary('f64', 'sub', 40, 2, 38);
testBinary('f64', 'mul', 40, 2, 80);
testBinary('f64', 'div', 40, 3, 13.333333333333334);
testBinary('f64', 'min', 40, 2, 2);
testBinary('f64', 'max', 40, 2, 40);
testBinary('f64', 'copysign', 40, -2, -40);

testComparison('f64', 'eq', 40, 40, 1);
testComparison('f64', 'ne', 40, 40, 0);
testComparison('f64', 'lt', 40, 40, 0);
testComparison('f64', 'le', 40, 40, 1);
testComparison('f64', 'gt', 40, 40, 0);
testComparison('f64', 'ge', 40, 40, 1);

assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result f32) (f32.sqrt (get_local 0))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (result i32) (f32.sqrt (get_local 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result i32) (f32.sqrt (get_local 0))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result f64) (f64.sqrt (get_local 0))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (result i32) (f64.sqrt (get_local 0))))'), TypeError, mismatchError("f64", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result i32) (f64.sqrt (get_local 0))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (f32.sqrt (nop))))'), TypeError, mismatchError("void", "f32"));

assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result f32) (f32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result f32) (f32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param f32) (result i32) (f32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result i32) (f32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f64) (result f64) (f64.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (param i32) (result f64) (f64.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (param f64) (result i32) (f64.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f64", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result i32) (f64.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));

assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result f32) (f32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result f32) (f32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param f32) (result f32) (f32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f64) (result f64) (f64.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (param i32) (result f64) (f64.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (param f64) (result f64) (f64.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));

// Non-canonical NaNs.
assertEq(wasmEvalText('(module (func (result i32) (i32.reinterpret/f32 (f32.mul (f32.const 0.0) (f32.const -nan:0x222222)))) (export "" 0))')(), -0x1dddde);
assertEq(wasmEvalText('(module (func (result i32) (i32.reinterpret/f32 (f32.min (f32.const 0.0) (f32.const -nan:0x222222)))) (export "" 0))')(), -0x1dddde);
assertEq(wasmEvalText('(module (func (result i32) (i32.reinterpret/f32 (f32.max (f32.const 0.0) (f32.const -nan:0x222222)))) (export "" 0))')(), -0x1dddde);
assertEq(wasmEvalText('(module (func (result i32) (local i64) (set_local 0 (i64.reinterpret/f64 (f64.mul (f64.const 0.0) (f64.const -nan:0x4444444444444)))) (i32.xor (i32.wrap/i64 (get_local 0)) (i32.wrap/i64 (i64.shr_u (get_local 0) (i64.const 32))))) (export "" 0))')(), -0x44480000);
assertEq(wasmEvalText('(module (func (result i32) (local i64) (set_local 0 (i64.reinterpret/f64 (f64.min (f64.const 0.0) (f64.const -nan:0x4444444444444)))) (i32.xor (i32.wrap/i64 (get_local 0)) (i32.wrap/i64 (i64.shr_u (get_local 0) (i64.const 32))))) (export "" 0))')(), -0x44480000);
assertEq(wasmEvalText('(module (func (result i32) (local i64) (set_local 0 (i64.reinterpret/f64 (f64.max (f64.const 0.0) (f64.const -nan:0x4444444444444)))) (i32.xor (i32.wrap/i64 (get_local 0)) (i32.wrap/i64 (i64.shr_u (get_local 0) (i64.const 32))))) (export "" 0))')(), -0x44480000);
