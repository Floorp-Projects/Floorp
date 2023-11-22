assertEq(wasmEvalText('(module (func (result i32) (i32.const -1)) (export "" (func 0)))').exports[""](), -1);
assertEq(wasmEvalText('(module (func (result i32) (i32.const -2147483648)) (export "" (func 0)))').exports[""](), -2147483648);
assertEq(wasmEvalText('(module (func (result i32) (i32.const 4294967295)) (export "" (func 0)))').exports[""](), -1);

function testUnary(type, opcode, op, expect) {
    if (type === 'i64') {
        // Test with constant
        wasmFullPassI64(`(module (func $run (result ${type}) (${type}.${opcode} (${type}.const ${op}))))`, expect);
        // Test with param
        wasmFullPassI64(`(module (func $run (param ${type}) (result ${type}) (${type}.${opcode} (local.get 0))))`, expect, {}, `i64.const ${op}`);
        return;
    }
    // Test with constant
    wasmFullPass(`(module (func (result ${type}) (${type}.${opcode} (${type}.const ${op}))) (export "run" (func 0)))`, expect);
    // Test with param
    wasmFullPass(`(module (func (param ${type}) (result ${type}) (${type}.${opcode} (local.get 0))) (export "run" (func 0)))`, expect, {}, op);
}

function testBinary64(opcode, lhs, rhs, expect) {
    let lsrc = `i64.const ${lhs}`;
    let rsrc = `i64.const ${rhs}`;
    wasmFullPassI64(`(module (func $run (param i64) (param i64) (result i64) (i64.${opcode} (local.get 0) (local.get 1))))`, expect, {}, lsrc, rsrc);
    // The same, but now the RHS is a constant.
    wasmFullPassI64(`(module (func $run (param i64) (result i64) (i64.${opcode} (local.get 0) (i64.const ${rhs}))))`, expect, {}, lsrc);
    // LHS and RHS are constants.
    wasmFullPassI64(`(module (func $run (result i64) (i64.${opcode} (i64.const ${lhs}) (i64.const ${rhs}))))`, expect);
}

function testBinary32(opcode, lhs, rhs, expect) {
    wasmFullPass(`(module (func (param i32) (param i32) (result i32) (i32.${opcode} (local.get 0) (local.get 1))) (export "run" (func 0)))`, expect, {}, lhs, rhs);
    // The same, but now the RHS is a constant.
    wasmFullPass(`(module (func (param i32) (result i32) (i32.${opcode} (local.get 0) (i32.const ${rhs}))) (export "run" (func 0)))`, expect, {}, lhs);
    // LHS and RHS are constants.
    wasmFullPass(`(module (func (result i32) (i32.${opcode} (i32.const ${lhs}) (i32.const ${rhs}))) (export "run" (func 0)))`, expect);
}

function testComparison32(opcode, lhs, rhs, expect) {
    wasmFullPass(`(module (func (param i32) (param i32) (result i32) (i32.${opcode} (local.get 0) (local.get 1))) (export "run" (func 0)))`, expect, {}, lhs, rhs);
}
function testComparison64(opcode, lhs, rhs, expect) {
    let lsrc = `i64.const ${lhs}`;
    let rsrc = `i64.const ${rhs}`;

    wasmFullPass(`(module
                    (func $cmp (param i64) (param i64) (result i32) (i64.${opcode} (local.get 0) (local.get 1)))
                    (func $assert (result i32)
                     i64.const ${lhs}
                     i64.const ${rhs}
                     call $cmp
                    )
                    (export "run" (func $assert)))`, expect);

    // Also test `if`, for the compare-and-branch path.
    wasmFullPass(`(module
                    (func $cmp (param i64) (param i64) (result i32)
                      (if (result i32) (i64.${opcode} (local.get 0) (local.get 1))
                       (then (i32.const 1))
                       (else (i32.const 0))))
                    (func $assert (result i32)
                     i64.const ${lhs}
                     i64.const ${rhs}
                     call $cmp
                    )
                    (export "run" (func $assert)))`, expect);
}

function testI64Eqz(input, expect) {
    wasmFullPass(`(module (func (result i32) (i64.eqz (i64.const ${input}))) (export "run" (func 0)))`, expect, {});
    wasmFullPass(`(module
        (func (param i64) (result i32) (i64.eqz (local.get 0)))
        (func $assert (result i32) (i64.const ${input}) (call 0))
        (export "run" (func $assert)))`, expect);
}

function testTrap32(opcode, lhs, rhs, expect) {
    assertErrorMessage(() => wasmEvalText(`(module (func (param i32) (param i32) (result i32) (i32.${opcode} (local.get 0) (local.get 1))) (export "" (func 0)))`).exports[""](lhs, rhs), Error, expect);
    // The same, but now the RHS is a constant.
    assertErrorMessage(() => wasmEvalText(`(module (func (param i32) (result i32) (i32.${opcode} (local.get 0) (i32.const ${rhs}))) (export "" (func 0)))`).exports[""](lhs), Error, expect);
    // LHS and RHS are constants.
    assertErrorMessage(wasmEvalText(`(module (func (result i32) (i32.${opcode} (i32.const ${lhs}) (i32.const ${rhs}))) (export "" (func 0)))`).exports[""], Error, expect);
}

function testTrap64(opcode, lhs, rhs, expect) {
    let $call = ``;

    assertErrorMessage(wasmEvalText(`(module
        (func (param i64) (param i64) (result i64) (i64.${opcode} (local.get 0) (local.get 1)))
        (func $f (export "") (i64.const ${lhs}) (i64.const ${rhs}) (call 0) drop)
    )`).exports[""], Error, expect);
    // The same, but now the RHS is a constant.
    assertErrorMessage(wasmEvalText(`(module
        (func (param i64) (result i64) (i64.${opcode} (local.get 0) (i64.const ${rhs})))
        (func $f (export "") (i64.const ${lhs}) (call 0) drop)
    )`).exports[""], Error, expect);
    // LHS and RHS are constants.
    assertErrorMessage(wasmEvalText(`(module
        (func (result i64) (i64.${opcode} (i64.const ${lhs}) (i64.const ${rhs})))
        (func $f (export "") (call 0) drop)
    )`).exports[""], Error, expect);
}

testUnary('i32', 'clz', 40, 26);
testUnary('i32', 'clz', 0, 32);
testUnary('i32', 'clz', 0xFFFFFFFF, 0);
testUnary('i32', 'clz', -2147483648, 0);

testUnary('i32', 'ctz', 40, 3);
testUnary('i32', 'ctz', 0, 32);
testUnary('i32', 'ctz', -2147483648, 31);

testUnary('i32', 'popcnt', 40, 2);
testUnary('i32', 'popcnt', 0, 0);
testUnary('i32', 'popcnt', 0xFFFFFFFF, 32);

testUnary('i32', 'eqz', 0, 1);
testUnary('i32', 'eqz', 1, 0);
testUnary('i32', 'eqz', 0xFFFFFFFF, 0);

testBinary32('add', 40, 2, 42);
testBinary32('sub', 40, 2, 38);
testBinary32('mul', 40, 2, 80);
testBinary32('div_s', -40, 2, -20);
testBinary32('div_s', -40, 8, -5);
testBinary32('div_s', -40, 7, -5);
testBinary32('div_s', 40, 8, 5);
testBinary32('div_s', 40, 7, 5);
testBinary32('div_u', -40, 2, 2147483628);
testBinary32('div_u', 40, 2, 20);
testBinary32('div_u', 40, 8, 5);
testBinary32('rem_s', 40, -3, 1);
testBinary32('rem_s', 0, -3, 0);
testBinary32('rem_s', 5, 2, 1);
testBinary32('rem_s', 65, 64, 1);
testBinary32('rem_s', -65, 64, -1);
testBinary32('rem_u', 40, -3, 40);
testBinary32('rem_u', 41, 8, 1);
testBinary32('and', 42, 6, 2);
testBinary32('or', 42, 6, 46);
testBinary32('xor', 42, 2, 40);
testBinary32('xor', -1, 1739168047, -1739168048);
testBinary32('xor', -1739168043, -1, 1739168042);
testBinary32('shl', 40, 0, 40);
testBinary32('shl', 40, 2, 160);
testBinary32('shr_s', -40, 0, -40);
testBinary32('shr_s', -40, 2, -10);
testBinary32('shr_u', -40, 0, -40);
testBinary32('shr_u', -40, 2, 1073741814);

testTrap32('div_s', 42, 0, /integer divide by zero/);
testTrap32('div_s', 0x80000000 | 0, -1, /integer overflow/);
testTrap32('div_u', 42, 0, /integer divide by zero/);
testTrap32('rem_s', 42, 0, /integer divide by zero/);
testTrap32('rem_u', 42, 0, /integer divide by zero/);

testBinary32('rotl', 40, 2, 160);
testBinary32('rotl', 40, 34, 160);
testBinary32('rotr', 40, 2, 10);
testBinary32('rotr', 40, 34, 10);
testBinary32('rotr', 40, 0, 40);
testBinary32('rotl', 40, 0, 40);

testComparison32('eq', 40, 40, 1);
testComparison32('ne', 40, 40, 0);
testComparison32('lt_s', 40, 40, 0);
testComparison32('lt_u', 40, 40, 0);
testComparison32('le_s', 40, 40, 1);
testComparison32('le_u', 40, 40, 1);
testComparison32('gt_s', 40, 40, 0);
testComparison32('gt_u', 40, 40, 0);
testComparison32('ge_s', 40, 40, 1);
testComparison32('ge_u', 40, 40, 1);

// On 32-bit debug builds, with --ion-eager, this test can run into our
// per-process JIT code limits and OOM. Trigger a GC to discard code.
if (getJitCompilerOptions()["ion.warmup.trigger"] === 0)
    gc();

// Test MTest's GVN branch inversion.
var testTrunc = wasmEvalText(`(module (func (param f32) (result i32) (if (result i32) (i32.eqz (i32.trunc_f32_s (local.get 0))) (then (i32.const 0)) (else (i32.const 1)))) (export "" (func 0)))`).exports[""];
assertEq(testTrunc(0), 0);
assertEq(testTrunc(13.37), 1);

testBinary64('add', 40, 2, 42);
testBinary64('add', "0x1234567887654321", -1, "0x1234567887654320");
testBinary64('add', "0xffffffffffffffff", 1, 0);
testBinary64('sub', 40, 2, 38);
testBinary64('sub', "0x1234567887654321", "0x123456789", "0x12345677641fdb98");
testBinary64('sub', 3, 5, -2);
testBinary64('mul', 40, 2, 80);
testBinary64('mul', -1, 2, -2);
testBinary64('mul', 0x123456, "0x9876543210", "0xad77d2c5f941160");
testBinary64('mul', 2, -1, -2);
testBinary64('mul', "0x80000000", -1, -2147483648);
testBinary64('mul', "0x7fffffff", -1, -2147483647);
testBinary64('mul', "0x7fffffffffffffff", -1, "0x8000000000000001");
testBinary64('mul', 2, 2, 4);
testBinary64('mul', "0x80000000", 2, "0x100000000");
testBinary64('mul', "0x7fffffff", 2, "0xfffffffe");
testBinary64('div_s', -40, 2, -20);
testBinary64('div_s', -40, 8, -5);
testBinary64('div_s', -40, 7, -5);
testBinary64('div_s', 40, 8, 5);
testBinary64('div_s', 40, 7, 5);
testBinary64('div_s', "0x1234567887654321", 2, "0x91a2b3c43b2a190");
testBinary64('div_s', "0x1234567887654321", "0x1000000000", "0x1234567");
testBinary64('div_s', -1, "0x100000000", 0);
testBinary64('div_u', -40, 2, "0x7fffffffffffffec");
testBinary64('div_u', "0x1234567887654321", 9, "0x205d0b80f0b4059");
testBinary64('div_u', 40, 2, 20);
testBinary64('div_u', 40, 8, 5);
testBinary64('rem_s', 40, -3, 1);
testBinary64('rem_s', 0, -3, 0);
testBinary64('rem_s', 5, 2, 1);
testBinary64('rem_s', 65, 64, 1);
testBinary64('rem_s', "0x1234567887654321", "0x1000000000", "0x887654321");
testBinary64('rem_s', "0x7fffffffffffffff", -1, 0);
testBinary64('rem_s', "0x8000000000000001", 1000, -807);
testBinary64('rem_s', "0x8000000000000000", -1, 0);
testBinary64('rem_u', 40, -3, 40);
testBinary64('rem_u', 41, 8, 1);
testBinary64('rem_u', "0x1234567887654321", "0x1000000000", "0x887654321");
testBinary64('rem_u', "0x8000000000000000", -1, "0x8000000000000000");
testBinary64('rem_u', "0x8ff00ff00ff00ff0", "0x100000001", "0x80000001");

testTrap64('div_s', 10, 0, /integer divide by zero/);
testTrap64('div_s', "0x8000000000000000", -1, /integer overflow/);
testTrap64('div_u', 0, 0, /integer divide by zero/);
testTrap64('rem_s', 10, 0, /integer divide by zero/);
testTrap64('rem_u', 10, 0, /integer divide by zero/);

// Bitops.
testBinary64('or', 42, 6, 46);
testBinary64('or', "0x8765432112345678", "0xffff0000ffff0000", "0xffff4321ffff5678");

testBinary64('xor', 42, 2, 40);
testBinary64('xor', "0x8765432112345678", "0xffff0000ffff0000", "0x789a4321edcb5678");
testBinary64('xor', "0xffffffffffffffff", "0x2c73173d985666d0", "0xd38ce8c267a9992f");
testBinary64('xor', "0xe8f3437fe059746a", "0xffffffffffffffff", "0x170cbc801fa68b95");

testBinary64('shl', 0xff00ff, 28, "0x0ff00ff0000000");
testBinary64('shl', 0xff00ff, 30, "0x3fc03fc0000000");
testBinary64('shl', 0xff00ff, 31, "0x7f807f80000000");
testBinary64('shl', 0xff00ff, 32, "0xff00ff00000000");
testBinary64('shl', 1, 63, "0x8000000000000000");
testBinary64('shl', 1, 64, 1);
testBinary64('shl', 40, 2, 160);
testBinary64('shl', 40, 0, 40);

testBinary64('shr_s', -40, 0, -40);
testBinary64('shr_s', -40, 2, -10);
testBinary64('shr_s', "0xff00ff0000000", 28, 0xff00ff);
testBinary64('shr_s', "0xff00ff0000000", 30, 0x3fc03f);
testBinary64('shr_s', "0xff00ff0000000", 31, 0x1fe01f);
testBinary64('shr_s', "0xff00ff0000000", 32, 0x0ff00f);

testBinary64('shr_u', -40, 0, -40);
testBinary64('shr_u', -40, 2, "0x3ffffffffffffff6");
testBinary64('shr_u', "0x8ffff00ff0000000", 30, "0x23fffc03f");
testBinary64('shr_u', "0x8ffff00ff0000000", 31, "0x11fffe01f");
testBinary64('shr_u', "0x8ffff00ff0000000", 32, "0x08ffff00f");
testBinary64('shr_u', "0x8ffff00ff0000000", 56, 0x8f);

testBinary64('and', 42, 0, 0);
testBinary64('and', 42, 6, 2);
testBinary64('and', "0x0000000012345678", "0xffff0000ffff0000", "0x0000000012340000");
testBinary64('and', "0x8765432112345678", "0xffff0000ffff0000", "0x8765000012340000");

// Rotations.
testBinary64('rotl', 40, 0, 0x28);
testBinary64('rotl', 40, 2, 0xA0);
testBinary64('rotl', 40, 8, 0x2800);
testBinary64('rotl', 40, 30, "0xA00000000");
testBinary64('rotl', 40, 31, "0x1400000000");
testBinary64('rotl', 40, 32, "0x2800000000");

testBinary64('rotl', "0x1234567812345678", 4, "0x2345678123456781");
testBinary64('rotl', "0x1234567812345678", 30, "0x048D159E048D159E");
testBinary64('rotl', "0x1234567812345678", 31, "0x091A2B3C091A2B3C");
testBinary64('rotl', "0x1234567812345678", 32, "0x1234567812345678");

testBinary64('rotl', "0x0000000000001000", 127, "0x0000000000000800");

testBinary64('rotr', 40, 0, 0x28);
testBinary64('rotr', 40, 2, 0x0A);
testBinary64('rotr', 40, 30, "0xA000000000");
testBinary64('rotr', 40, 31, "0x5000000000");
testBinary64('rotr', 40, 32, "0x2800000000");

testBinary64('rotr', "0x1234567812345678", 4, "0x8123456781234567");
testBinary64('rotr', "0x1234567812345678", 30, "0x48D159E048D159E0");
testBinary64('rotr', "0x1234567812345678", 31, "0x2468ACF02468ACF0");
testBinary64('rotr', "0x1234567812345678", 32, "0x1234567812345678");
testBinary64('rotr', "0x1234567812345678", 60, "0x2345678123456781");

testBinary64('rotr', "0x0000000000001000", 127, "0x0000000000002000");

// Comparisons.
testComparison64('eq', 40, 40, 1);
testComparison64('ne', 40, 40, 0);
testComparison64('lt_s', 40, 40, 0);
testComparison64('lt_u', 40, 40, 0);
testComparison64('le_s', 40, 40, 1);
testComparison64('le_u', 40, 40, 1);
testComparison64('gt_s', 40, 40, 0);
testComparison64('gt_u', 40, 40, 0);
testComparison64('ge_s', 40, 40, 1);
testComparison64('ge_u', 40, 40, 1);
testComparison64('eq', "0x400012345678", "0x400012345678", 1);
testComparison64('eq', "0x400012345678", "0x500012345678", 0);
testComparison64('ne', "0x400012345678", "0x400012345678", 0);
testComparison64('ne', "0x400012345678", "0x500012345678", 1);
testComparison64('eq', "0xffffffffffffffff", -1, 1);
testComparison64('lt_s', "0x8000000012345678", "0x1", 1);
testComparison64('lt_s', "0x1", "0x8000000012345678", 0);
testComparison64('lt_u', "0x8000000012345678", "0x1", 0);
testComparison64('lt_u', "0x1", "0x8000000012345678", 1);
testComparison64('le_s', -1, 0, 1);
testComparison64('le_s', -1, -1, 1);
testComparison64('le_s', 0, -1, 0);
testComparison64('le_u', -1, 0, 0);
testComparison64('le_u', -1, -1, 1);
testComparison64('le_u', 0, -1, 1);
testComparison64('gt_s', 1, "0x8000000000000000", 1);
testComparison64('gt_s', "0x8000000000000000", 1, 0);
testComparison64('gt_u', 1, "0x8000000000000000", 0);
testComparison64('gt_u', "0x8000000000000000", 1, 1);
testComparison64('ge_s', 1, "0x8000000000000000", 1);
testComparison64('ge_s', "0x8000000000000000", 1, 0);
testComparison64('ge_u', 1, "0x8000000000000000", 0);
testComparison64('ge_u', "0x8000000000000000", 1, 1);

testUnary('i64', 'clz', 40, 58);
testUnary('i64', 'clz', "0x8000000000000000", 0);
testUnary('i64', 'clz', "0x7fffffffffffffff", 1);
testUnary('i64', 'clz', "0x4000000000000000", 1);
testUnary('i64', 'clz', "0x3000000000000000", 2);
testUnary('i64', 'clz', "0x2000000000000000", 2);
testUnary('i64', 'clz', "0x1000000000000000", 3);
testUnary('i64', 'clz', "0x0030000000000000", 10);
testUnary('i64', 'clz', "0x0000800000000000", 16);
testUnary('i64', 'clz', "0x00000000ffffffff", 32);
testUnary('i64', 'clz', -1, 0);
testUnary('i64', 'clz', 0, 64);

testUnary('i64', 'ctz', 40, 3);
testUnary('i64', 'ctz', "0x8000000000000000", 63);
testUnary('i64', 'ctz', "0x7fffffffffffffff", 0);
testUnary('i64', 'ctz', "0x4000000000000000", 62);
testUnary('i64', 'ctz', "0x3000000000000000", 60);
testUnary('i64', 'ctz', "0x2000000000000000", 61);
testUnary('i64', 'ctz', "0x1000000000000000", 60);
testUnary('i64', 'ctz', "0x0030000000000000", 52);
testUnary('i64', 'ctz', "0x0000800000000000", 47);
testUnary('i64', 'ctz', "0x00000000ffffffff", 0);
testUnary('i64', 'ctz', -1, 0);
testUnary('i64', 'ctz', 0, 64);

testUnary('i64', 'popcnt', 40, 2);
testUnary('i64', 'popcnt', 0, 0);
testUnary('i64', 'popcnt', "0x8000000000000000", 1);
testUnary('i64', 'popcnt', "0x7fffffffffffffff", 63);
testUnary('i64', 'popcnt', "0x4000000000000000", 1);
testUnary('i64', 'popcnt', "0x3000000000000000", 2);
testUnary('i64', 'popcnt', "0x2000000000000000", 1);
testUnary('i64', 'popcnt', "0x1000000000000000", 1);
testUnary('i64', 'popcnt', "0x0030000000000000", 2);
testUnary('i64', 'popcnt', "0x0000800000000000", 1);
testUnary('i64', 'popcnt', "0x00000000ffffffff", 32);
testUnary('i64', 'popcnt', -1, 64);
testUnary('i64', 'popcnt', 0, 0);

testI64Eqz(40, 0);
testI64Eqz(0, 1);

wasmAssert(`(module (func $run (param i64) (result i64) (local i64) (local.set 1 (i64.shl (local.get 0) (local.get 0))) (i64.shl (local.get 1) (local.get 1))))`,
           [{ type: 'i64', func: '$run', args: ['i64.const 2'], expected: 2048}]);

// Test MTest's GVN branch inversion.
var testTrunc = wasmEvalText(`(module (func (param f32) (result i32) (if (result i32) (i64.eqz (i64.trunc_f32_s (local.get 0))) (then (i32.const 0)) (else (i32.const 1)))) (export "" (func 0)))`).exports[""];
assertEq(testTrunc(0), 0);
assertEq(testTrunc(13.37), 1);

wasmAssert(`(module (func $run (result i64) (local i64) (local.set 0 (i64.rem_s (i64.const 1) (i64.const 0xf))) (i64.rem_s (local.get 0) (local.get 0))))`,
           [{ type: 'i64', func: '$run', expected: 0 }]);

wasmFailValidateText('(module (func (param f32) (result i32) (i32.clz (local.get 0))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (param i32) (result f32) (i32.clz (local.get 0))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param f32) (result f32) (i32.clz (local.get 0))))', mismatchError("f32", "i32"));

wasmFailValidateText('(module (func (param f32) (param i32) (result i32) (i32.add (local.get 0) (local.get 1))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (param i32) (param f32) (result i32) (i32.add (local.get 0) (local.get 1))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (param i32) (param i32) (result f32) (i32.add (local.get 0) (local.get 1))))', mismatchError("i32", "f32"));
wasmFailValidateText('(module (func (param f32) (param f32) (result f32) (i32.add (local.get 0) (local.get 1))))', mismatchError("f32", "i32"));

wasmFailValidateText('(module (func (param f32) (param i32) (result i32) (i32.eq (local.get 0) (local.get 1))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (param i32) (param f32) (result i32) (i32.eq (local.get 0) (local.get 1))))', mismatchError("f32", "i32"));
wasmFailValidateText('(module (func (param i32) (param i32) (result f32) (i32.eq (local.get 0) (local.get 1))))', mismatchError("i32", "f32"));
