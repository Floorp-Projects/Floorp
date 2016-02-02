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

assertErrorMessage(() => wasmEvalText('(module (func (param f32) (result i32) (i32.popcnt (get_local 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result f32) (i32.popcnt (get_local 0))))'), TypeError, mismatchError("i32", "f32"));

assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result i32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result i32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result f32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
