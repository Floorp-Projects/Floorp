load(libdir + "wasm.js");

if (!wasmIsSupported())
    quit();

function mismatchError(actual, expect) {
    var str = "type mismatch: expression has type " + actual + " but expected " + expect;
    return RegExp(str);
}

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
//testUnary('f32', 'nearest', -41.5, -42); // TODO: NYI
//testUnary('f32', 'trunc', -41.5, -41); // TODO: NYI
testUnary('f32', 'sqrt', 40, 6.324555397033691);

testBinary('f32', 'add', 40, 2, 42);
testBinary('f32', 'sub', 40, 2, 38);
testBinary('f32', 'mul', 40, 2, 80);
testBinary('f32', 'div', 40, 3, 13.333333015441895);
testBinary('f32', 'min', 40, 2, 2);
testBinary('f32', 'max', 40, 2, 40);
//testBinary('f32', 'copysign', 40, -2, -40); // TODO: NYI

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
//testUnary('f64', 'nearest', -41.5, -42); // TODO: NYI
//testUnary('f64', 'trunc', -41.5, -41); // TODO: NYI
testUnary('f64', 'sqrt', 40, 6.324555320336759);

testBinary('f64', 'add', 40, 2, 42);
testBinary('f64', 'sub', 40, 2, 38);
testBinary('f64', 'mul', 40, 2, 80);
testBinary('f64', 'div', 40, 3, 13.333333333333334);
testBinary('f64', 'min', 40, 2, 2);
testBinary('f64', 'max', 40, 2, 40);
//testBinary('f64', 'copysign', 40, -2, -40); // TODO: NYI

testComparison('f64', 'eq', 40, 40, 1);
testComparison('f64', 'ne', 40, 40, 0);
testComparison('f64', 'lt', 40, 40, 0);
testComparison('f64', 'le', 40, 40, 1);
testComparison('f64', 'gt', 40, 40, 0);
testComparison('f64', 'ge', 40, 40, 1);

assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result f32) (f32.sqrt (get_local 0))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (result i32) (f32.sqrt (get_local 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result i32) (f32.sqrt (get_local 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result f64) (f64.sqrt (get_local 0))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (result i32) (f64.sqrt (get_local 0))))'), TypeError, mismatchError("f64", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result i32) (f64.sqrt (get_local 0))))'), TypeError, mismatchError("f64", "i32"));

assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result f32) (f32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result f32) (f32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param f32) (result i32) (f32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result i32) (f32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f64) (result f64) (f64.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (param i32) (result f64) (f64.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (param f64) (result i32) (f64.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f64", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result i32) (f64.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f64", "i32"));

assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result f32) (f32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result f32) (f32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param f32) (result f32) (f32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f64) (result f64) (f64.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (param i32) (result f64) (f64.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (func (param f64) (param f64) (result f64) (f64.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f64"));
