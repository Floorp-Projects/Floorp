wasmFullPass('(module (func (result f32) (f32.const -1)) (export "run" 0))', -1);
wasmFullPass('(module (func (result f32) (f32.const 1)) (export "run" 0))', 1);
wasmFullPass('(module (func (result f64) (f64.const -2)) (export "run" 0))', -2);
wasmFullPass('(module (func (result f64) (f64.const 2)) (export "run" 0))', 2);
wasmFullPass('(module (func (result f64) (f64.const 4294967296)) (export "run" 0))', 4294967296);
wasmFullPass('(module (func (result f32) (f32.const 1.5)) (export "run" 0))', 1.5);
wasmFullPass('(module (func (result f64) (f64.const 2.5)) (export "run" 0))', 2.5);
wasmFullPass('(module (func (result f64) (f64.const 10e2)) (export "run" 0))', 10e2);
wasmFullPass('(module (func (result f32) (f32.const 10e2)) (export "run" 0))', 10e2);
wasmFullPass('(module (func (result f64) (f64.const -0x8000000000000000)) (export "run" 0))', -0x8000000000000000);
wasmFullPass('(module (func (result f64) (f64.const -9223372036854775808)) (export "run" 0))', -9223372036854775808);
wasmFullPass('(module (func (result f64) (f64.const 1797693134862315708145274e284)) (export "run" 0))', 1797693134862315708145274e284);

function testUnary(type, opcode, op, expect) {
    wasmFullPass('(module (func (param ' + type + ') (result ' + type + ') (' + type + '.' + opcode + ' (local.get 0))) (export "run" 0))',
                 expect,
                 {},
                 op);
}

function testBinary(type, opcode, lhs, rhs, expect) {
    wasmFullPass('(module (func (param ' + type + ') (param ' + type + ') (result ' + type + ') (' + type + '.' + opcode + ' (local.get 0) (local.get 1))) (export "run" 0))',
                 expect,
                 {},
                 lhs, rhs);
}

function testComparison(type, opcode, lhs, rhs, expect) {
    wasmFullPass('(module (func (param ' + type + ') (param ' + type + ') (result i32) (' + type + '.' + opcode + ' (local.get 0) (local.get 1))) (export "run" 0))',
                 expect,
                 {},
                 lhs, rhs);
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

wasmFailValidateText('(module (func (param i32) (result f32) (f32.sqrt (local.get 0))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param f32) (result i32) (f32.sqrt (local.get 0))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (param i32) (result i32) (f32.sqrt (local.get 0))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param i32) (result f64) (f64.sqrt (local.get 0))))', mismatchError("i32", "f64"));
wasmFailValidateText('(module (func (param f64) (result i32) (f64.sqrt (local.get 0))))', mismatchError("f64", "i32"));
wasmFailValidateText('(module (func (param i32) (result i32) (f64.sqrt (local.get 0))))', mismatchError("i32", "f64"));
wasmFailValidateText('(module (func (f32.sqrt (nop))))', /popping value from empty stack/);

wasmFailValidateText('(module (func (param i32) (param f32) (result f32) (f32.add (local.get 0) (local.get 1))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param f32) (param i32) (result f32) (f32.add (local.get 0) (local.get 1))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param f32) (param f32) (result i32) (f32.add (local.get 0) (local.get 1))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (param i32) (param i32) (result i32) (f32.add (local.get 0) (local.get 1))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param i32) (param f64) (result f64) (f64.add (local.get 0) (local.get 1))))', mismatchError("i32", "f64"));
wasmFailValidateText('(module (func (param f64) (param i32) (result f64) (f64.add (local.get 0) (local.get 1))))', mismatchError("i32", "f64"));
wasmFailValidateText('(module (func (param f64) (param f64) (result i32) (f64.add (local.get 0) (local.get 1))))', mismatchError("f64", "i32"));
wasmFailValidateText('(module (func (param i32) (param i32) (result i32) (f64.add (local.get 0) (local.get 1))))', mismatchError("i32", "f64"));

wasmFailValidateText('(module (func (param i32) (param f32) (result f32) (f32.eq (local.get 0) (local.get 1))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param f32) (param i32) (result f32) (f32.eq (local.get 0) (local.get 1))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param f32) (param f32) (result f32) (f32.eq (local.get 0) (local.get 1))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param i32) (param f64) (result f64) (f64.eq (local.get 0) (local.get 1))))', mismatchError("i32", "f64"));
wasmFailValidateText('(module (func (param f64) (param i32) (result f64) (f64.eq (local.get 0) (local.get 1))))', mismatchError("i32", "f64"));
wasmFailValidateText('(module (func (param f64) (param f64) (result f64) (f64.eq (local.get 0) (local.get 1))))', mismatchError("i32", "f64"));
