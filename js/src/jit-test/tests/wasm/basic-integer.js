load(libdir + "wasm.js");

function testUnary(type, opcode, op, expect) {
  assertEq(wasmEvalText('(module (func (param ' + type + ') (result ' + type + ') (' + type + '.' + opcode + ' (get_local 0))) (export "" 0))')(op), expect);
}

function testBinary(type, opcode, lhs, rhs, expect) {
  assertEq(wasmEvalText('(module (func (param ' + type + ') (param ' + type + ') (result ' + type + ') (' + type + '.' + opcode + ' (get_local 0) (get_local 1))) (export "" 0))')(lhs, rhs), expect);
}

function testComparison(type, opcode, lhs, rhs, expect) {
  assertEq(wasmEvalText('(module (func (param ' + type + ') (param ' + type + ') (result i32) (' + type + '.' + opcode + ' (get_local 0) (get_local 1))) (export "" 0))')(lhs, rhs), expect);
}

testUnary('i32', 'clz', 40, 26);
//testUnary('i32', 'ctz', 40, 0); // TODO: NYI
//testUnary('i32', 'popcnt', 40, 0); // TODO: NYI

testBinary('i32', 'add', 40, 2, 42);
testBinary('i32', 'sub', 40, 2, 38);
testBinary('i32', 'mul', 40, 2, 80);
testBinary('i32', 'div_s', -40, 2, -20);
testBinary('i32', 'div_u', -40, 2, 2147483628);
testBinary('i32', 'rem_s', 40, -3, 1);
testBinary('i32', 'rem_u', 40, -3, 40);
testBinary('i32', 'and', 42, 6, 2);
testBinary('i32', 'or', 42, 6, 46);
testBinary('i32', 'xor', 42, 2, 40);
testBinary('i32', 'shl', 40, 2, 160);
testBinary('i32', 'shr_s', -40, 2, -10);
testBinary('i32', 'shr_u', -40, 2, 1073741814);

testComparison('i32', 'eq', 40, 40, 1);
testComparison('i32', 'ne', 40, 40, 0);
testComparison('i32', 'lt_s', 40, 40, 0);
testComparison('i32', 'lt_u', 40, 40, 0);
testComparison('i32', 'le_s', 40, 40, 1);
testComparison('i32', 'le_u', 40, 40, 1);
testComparison('i32', 'gt_s', 40, 40, 0);
testComparison('i32', 'gt_u', 40, 40, 0);
testComparison('i32', 'ge_s', 40, 40, 1);
testComparison('i32', 'ge_u', 40, 40, 1);

//testUnary('i64', 'clz', 40, 58); // TODO: NYI
//testUnary('i64', 'ctz', 40, 0); // TODO: NYI
//testUnary('i64', 'popcnt', 40, 0); // TODO: NYI

//testBinary('i64', 'add', 40, 2, 42); // TODO: NYI
//testBinary('i64', 'sub', 40, 2, 38); // TODO: NYI
//testBinary('i64', 'mul', 40, 2, 80); // TODO: NYI
//testBinary('i64', 'div_s', -40, 2, -20); // TODO: NYI
//testBinary('i64', 'div_u', -40, 2, 2147483628); // TODO: NYI
//testBinary('i64', 'rem_s', 40, -3, 1); // TODO: NYI
//testBinary('i64', 'rem_u', 40, -3, 40); // TODO: NYI
//testBinary('i64', 'and', 42, 6, 2); // TODO: NYI
//testBinary('i64', 'or', 42, 6, 46); // TODO: NYI
//testBinary('i64', 'xor', 42, 2, 40); // TODO: NYI
//testBinary('i64', 'shl', 40, 2, 160); // TODO: NYI
//testBinary('i64', 'shr_s', -40, 2, -10); // TODO: NYI
//testBinary('i64', 'shr_u', -40, 2, 1073741814); // TODO: NYI

//testComparison('i64', 'eq', 40, 40, 1); // TODO: NYI
//testComparison('i64', 'ne', 40, 40, 0); // TODO: NYI
//testComparison('i64', 'lt_s', 40, 40, 0); // TODO: NYI
//testComparison('i64', 'lt_u', 40, 40, 0); // TODO: NYI
//testComparison('i64', 'le_s', 40, 40, 1); // TODO: NYI
//testComparison('i64', 'le_u', 40, 40, 1); // TODO: NYI
//testComparison('i64', 'gt_s', 40, 40, 0); // TODO: NYI
//testComparison('i64', 'gt_u', 40, 40, 0); // TODO: NYI
//testComparison('i64', 'ge_s', 40, 40, 1); // TODO: NYI
//testComparison('i64', 'ge_u', 40, 40, 1); // TODO: NYI

assertErrorMessage(() => wasmEvalText('(module (func (param f32) (result i32) (i32.clz (get_local 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result f32) (i32.clz (get_local 0))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (result f32) (i32.clz (get_local 0))))'), TypeError, mismatchError("i32", "f32"));

assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result i32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result i32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result f32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param f32) (result f32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));

assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result i32) (i32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result i32) (i32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result f32) (i32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
