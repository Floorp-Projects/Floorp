load(libdir + "wasm.js");

assertEq(wasmEvalText('(module (func (result i32) (i32.const -1)) (export "" 0))')(), -1);
assertEq(wasmEvalText('(module (func (result i32) (i32.const -2147483648)) (export "" 0))')(), -2147483648);
assertEq(wasmEvalText('(module (func (result i32) (i32.const 4294967295)) (export "" 0))')(), -1);

function testUnary(type, opcode, op, expect) {
  assertEq(wasmEvalText('(module (func (param ' + type + ') (result ' + type + ') (' + type + '.' + opcode + ' (get_local 0))) (export "" 0))')(op), expect);
}

function testBinary(type, opcode, lhs, rhs, expect) {
  if (type === 'i64') {
    // i64 cannot be imported/exported, so we use a wrapper function.
    assertEq(wasmEvalText(`(module
                            (func (param i64) (param i64) (result i64) (i64.${opcode} (get_local 0) (get_local 1)))
                            (func (result i32) (i64.eq (call 0 (i64.const ${lhs}) (i64.const ${rhs})) (i64.const ${expect})))
                            (export "" 1))`)(), 1);
    // The same, but now the RHS is a constant.
    assertEq(wasmEvalText(`(module
                            (func (param i64) (result i64) (i64.${opcode} (get_local 0) (i64.const ${rhs})))
                            (func (result i32) (i64.eq (call 0 (i64.const ${lhs})) (i64.const ${expect})))
                            (export "" 1))`)(), 1);
    // LHS and RHS are constants.
    assertEq(wasmEvalText(`(module
                            (func (result i64) (i64.${opcode} (i64.const ${lhs}) (i64.const ${rhs})))
                            (func (result i32) (i64.eq (call 0) (i64.const ${expect})))
                            (export "" 1))`)(), 1);
  } else {
    assertEq(wasmEvalText('(module (func (param ' + type + ') (param ' + type + ') (result ' + type + ') (' + type + '.' + opcode + ' (get_local 0) (get_local 1))) (export "" 0))')(lhs, rhs), expect);
  }
}

function testComparison(type, opcode, lhs, rhs, expect) {
  if (type === 'i64') {
    // i64 cannot be imported/exported, so we use a wrapper function.
    assertEq(wasmEvalText(`(module
                            (func (param i64) (param i64) (result i32) (i64.${opcode} (get_local 0) (get_local 1)))
                            (func (result i32) (call 0 (i64.const ${lhs}) (i64.const ${rhs})))
                            (export "" 1))`)(), expect);
    // Also test if_else, for the compare-and-branch path.
    assertEq(wasmEvalText(`(module
                            (func (param i64) (param i64) (result i32)
                              (if_else (i64.${opcode} (get_local 0) (get_local 1))
                                (i32.const 1)
                                (i32.const 0)))
                            (func (result i32) (call 0 (i64.const ${lhs}) (i64.const ${rhs})))
                            (export "" 1))`)(), expect);
  } else {
    assertEq(wasmEvalText('(module (func (param ' + type + ') (param ' + type + ') (result i32) (' + type + '.' + opcode + ' (get_local 0) (get_local 1))) (export "" 0))')(lhs, rhs), expect);
  }
}

testUnary('i32', 'clz', 40, 26);
testUnary('i32', 'ctz', 40, 3);
testUnary('i32', 'ctz', 0, 32);
testUnary('i32', 'ctz', -2147483648, 31);

testUnary('i32', 'popcnt', 40, 2);
testUnary('i32', 'popcnt', 0, 0);
testUnary('i32', 'popcnt', 0xFFFFFFFF, 32);

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

if (getBuildConfiguration().x64) {
    testBinary('i64', 'add', 40, 2, 42);
    testBinary('i64', 'add', "0x1234567887654321", -1, "0x1234567887654320");
    testBinary('i64', 'add', "0xffffffffffffffff", 1, 0);
    testBinary('i64', 'sub', 40, 2, 38);
    testBinary('i64', 'sub', "0x1234567887654321", "0x123456789", "0x12345677641fdb98");
    testBinary('i64', 'sub', 3, 5, -2);
    testBinary('i64', 'mul', 40, 2, 80);
    testBinary('i64', 'mul', -1, 2, -2);
    testBinary('i64', 'mul', 0x123456, "0x9876543210", "0xad77d2c5f941160");
    testBinary('i64', 'div_s', -40, 2, -20);
    testBinary('i64', 'div_s', "0x1234567887654321", 2, "0x91a2b3c43b2a190");
    testBinary('i64', 'div_s', "0x1234567887654321", "0x1000000000", "0x1234567");
    testBinary('i64', 'div_u', -40, 2, "0x7fffffffffffffec");
    testBinary('i64', 'div_u', "0x1234567887654321", 9, "0x205d0b80f0b4059");
    testBinary('i64', 'rem_s', 40, -3, 1);
    testBinary('i64', 'rem_s', "0x1234567887654321", "0x1000000000", "0x887654321");
    testBinary('i64', 'rem_s', "0x7fffffffffffffff", -1, 0);
    testBinary('i64', 'rem_s', "0x8000000000000001", 1000, -807);
    testBinary('i64', 'rem_s', "0x8000000000000000", -1, 0);
    testBinary('i64', 'rem_u', 40, -3, 40);
    testBinary('i64', 'rem_u', "0x1234567887654321", "0x1000000000", "0x887654321");
    testBinary('i64', 'rem_u', "0x8000000000000000", -1, "0x8000000000000000");
    testBinary('i64', 'rem_u', "0x8ff00ff00ff00ff0", "0x100000001", "0x80000001");

    // These should trap, but for now we match the i32 version.
    testBinary('i64', 'div_s', 10, 0, 0);
    testBinary('i64', 'div_s', "0x8000000000000000", -1, "0x8000000000000000");
    testBinary('i64', 'div_u', 0, 0, 0);
    testBinary('i64', 'rem_s', 10, 0, 0);
    testBinary('i64', 'rem_u', 10, 0, 0);

    testBinary('i64', 'and', 42, 6, 2);
    testBinary('i64', 'or', 42, 6, 46);
    testBinary('i64', 'xor', 42, 2, 40);
    testBinary('i64', 'and', "0x8765432112345678", "0xffff0000ffff0000", "0x8765000012340000");
    testBinary('i64', 'or', "0x8765432112345678", "0xffff0000ffff0000", "0xffff4321ffff5678");
    testBinary('i64', 'xor', "0x8765432112345678", "0xffff0000ffff0000", "0x789a4321edcb5678");
    testBinary('i64', 'shl', 40, 2, 160);
    testBinary('i64', 'shr_s', -40, 2, -10);
    testBinary('i64', 'shr_u', -40, 2, "0x3ffffffffffffff6");
    testBinary('i64', 'shl', 0xff00ff, 28, "0xff00ff0000000");
    testBinary('i64', 'shl', 1, 63, "0x8000000000000000");
    testBinary('i64', 'shl', 1, 64, 1);
    testBinary('i64', 'shr_s', "0xff00ff0000000", 28, 0xff00ff);
    testBinary('i64', 'shr_u', "0x8ffff00ff0000000", 56, 0x8f);

    testComparison('i64', 'eq', 40, 40, 1);
    testComparison('i64', 'ne', 40, 40, 0);
    testComparison('i64', 'lt_s', 40, 40, 0);
    testComparison('i64', 'lt_u', 40, 40, 0);
    testComparison('i64', 'le_s', 40, 40, 1);
    testComparison('i64', 'le_u', 40, 40, 1);
    testComparison('i64', 'gt_s', 40, 40, 0);
    testComparison('i64', 'gt_u', 40, 40, 0);
    testComparison('i64', 'ge_s', 40, 40, 1);
    testComparison('i64', 'ge_u', 40, 40, 1);
    testComparison('i64', 'eq', "0x400012345678", "0x400012345678", 1);
    testComparison('i64', 'ne', "0x400012345678", "0x400012345678", 0);
    testComparison('i64', 'ne', "0x400012345678", "0x500012345678", 1);
    testComparison('i64', 'eq', "0xffffffffffffffff", "-1", 1);
    testComparison('i64', 'lt_s', "0x8000000012345678", "0x1", 1);
    testComparison('i64', 'lt_u', "0x8000000012345678", "0x1", 0);
    testComparison('i64', 'le_s', "-1", "0", 1);
    testComparison('i64', 'le_u', "-1", "-1", 1);
    testComparison('i64', 'gt_s', "1", "0x8000000000000000", 1);
    testComparison('i64', 'gt_u', "1", "0x8000000000000000", 0);
    testComparison('i64', 'ge_s', "1", "0x8000000000000000", 1);
    testComparison('i64', 'ge_u', "1", "0x8000000000000000", 0);
} else {
    // Sleeper test: once i64 works on more platforms, remove this if-else.
    try {
        testComparison('i64', 'eq', 40, 40, 1);
        assertEq(0, 1);
    } catch(e) {
        assertEq(e.toString().indexOf("NYI on this platform") >= 0, true);
    }
}

assertErrorMessage(() => wasmEvalText('(module (func (param f32) (result i32) (i32.clz (get_local 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result f32) (i32.clz (get_local 0))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (result f32) (i32.clz (get_local 0))))'), TypeError, mismatchError("f32", "i32"));

assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result i32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result i32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result f32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param f32) (result f32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));

assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result i32) (i32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result i32) (i32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result f32) (i32.eq (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
