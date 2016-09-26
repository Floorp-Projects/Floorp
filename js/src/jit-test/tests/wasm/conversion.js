// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

function testConversion(resultType, opcode, paramType, op, expect) {
  if (paramType === 'i64') {
    // i64 cannot be imported, so we use a wrapper function.
    assertEq(wasmEvalText(`(module
                            (func (param i64) (result ${resultType}) (${resultType}.${opcode}/i64 (get_local 0)))
                            (export "" 0))`).exports[""](createI64(op)), expect);
    // The same, but now the input is a constant.
    assertEq(wasmEvalText(`(module
                            (func (result ${resultType}) (${resultType}.${opcode}/i64 (i64.const ${op})))
                            (export "" 0))`).exports[""](), expect);
  } else if (resultType === 'i64') {
    assertEqI64(wasmEvalText(`(module
                            (func (param ${paramType}) (result i64) (i64.${opcode}/${paramType} (get_local 0)))
                            (export "" 0))`).exports[""](op), createI64(expect));
    // The same, but now the input is a constant.
    assertEqI64(wasmEvalText(`(module
                            (func (result i64) (i64.${opcode}/${paramType} (${paramType}.const ${op})))
                            (export "" 0))`).exports[""](), createI64(expect));
  } else {
    assertEq(wasmEvalText('(module (func (param ' + paramType + ') (result ' + resultType + ') (' + resultType + '.' + opcode + '/' + paramType + ' (get_local 0))) (export "" 0))').exports[""](op), expect);
  }

  let formerTestMode = getJitCompilerOptions()['wasm.test-mode'];
  setJitCompilerOption('wasm.test-mode', 1);
  for (var bad of ['i32', 'f32', 'f64', 'i64']) {
      if (bad != resultType) {
          wasmFailValidateText(
              `(module (func (param ${paramType}) (result ${bad}) (${resultType}.${opcode}/${paramType} (get_local 0))))`,
              mismatchError(resultType, bad)
          );
      }

      if (bad != paramType) {
          wasmFailValidateText(
              `(module (func (param ${bad}) (result ${resultType}) (${resultType}.${opcode}/${paramType} (get_local 0))))`,
              mismatchError(bad, paramType)
          );
      }
  }
  setJitCompilerOption('wasm.test-mode', formerTestMode);
}

function testTrap(resultType, opcode, paramType, op, expect) {
    let func = wasmEvalText(`(module
        (func
            (param ${paramType})
            (result ${resultType})
            (${resultType}.${opcode}/${paramType} (get_local 0))
        )
        (export "" 0)
    )`).exports[""];

    let expectedError = op === 'nan' ? /invalid conversion to integer/ : /integer overflow/;

    assertErrorMessage(() => func(jsify(op)), Error, expectedError);
}

{
    setJitCompilerOption('wasm.test-mode', 1);

    testConversion('i32', 'wrap', 'i64', '0x100000028', 40);
    testConversion('i32', 'wrap', 'i64', -10, -10);
    testConversion('i32', 'wrap', 'i64', "0xffffffff7fffffff", 0x7fffffff);
    testConversion('i32', 'wrap', 'i64', "0xffffffff00000000", 0);
    testConversion('i32', 'wrap', 'i64', "0xfffffffeffffffff", -1);
    testConversion('i32', 'wrap', 'i64', "0x1234567801abcdef", 0x01abcdef);
    testConversion('i32', 'wrap', 'i64', "0x8000000000000002", 2);

    testConversion('i64', 'extend_s', 'i32', 0, 0);
    testConversion('i64', 'extend_s', 'i32', 1234, 1234);
    testConversion('i64', 'extend_s', 'i32', -567, -567);
    testConversion('i64', 'extend_s', 'i32', 0x7fffffff, "0x000000007fffffff");
    testConversion('i64', 'extend_s', 'i32', 0x80000000, "0xffffffff80000000");

    testConversion('i64', 'extend_u', 'i32', 0, 0);
    testConversion('i64', 'extend_u', 'i32', 1234, 1234);
    testConversion('i64', 'extend_u', 'i32', -567, "0x00000000fffffdc9");
    testConversion('i64', 'extend_u', 'i32', -1, "0x00000000ffffffff");
    testConversion('i64', 'extend_u', 'i32', 0x7fffffff, "0x000000007fffffff");
    testConversion('i64', 'extend_u', 'i32', 0x80000000, "0x0000000080000000");

    testConversion('f32', 'convert_s', 'i64', 1, 1.0);
    testConversion('f32', 'convert_s', 'i64', -1, -1.0);
    testConversion('f32', 'convert_s', 'i64', 0, 0.0);
    testConversion('f32', 'convert_s', 'i64', "0x7fffffffffffffff", 9223372036854775807.0);
    testConversion('f32', 'convert_s', 'i64', "0x8000000000000000", -9223372036854775808.0);
    testConversion('f32', 'convert_s', 'i64', "0x11db9e76a2483", 314159275180032.0);
    testConversion('f32', 'convert_s', 'i64', "0x7fffffff", 2147483648.0); // closesth approx.
    testConversion('f32', 'convert_s', 'i64', "0x80000000", 2147483648.0);
    testConversion('f32', 'convert_s', 'i64', "0x80000001", 2147483648.0); // closesth approx.

    testConversion('f64', 'convert_s', 'i64', 1, 1.0);
    testConversion('f64', 'convert_s', 'i64', -1, -1.0);
    testConversion('f64', 'convert_s', 'i64', 0, 0.0);
    testConversion('f64', 'convert_s', 'i64', "0x7fffffffffffffff", 9223372036854775807.0);
    testConversion('f64', 'convert_s', 'i64', "0x8000000000000000", -9223372036854775808.0);
    testConversion('f64', 'convert_s', 'i64', "0x10969d374b968e", 4669201609102990);
    testConversion('f64', 'convert_s', 'i64', "0x7fffffff", 2147483647.0);
    testConversion('f64', 'convert_s', 'i64', "0x80000000", 2147483648.0);
    testConversion('f64', 'convert_s', 'i64', "0x80000001", 2147483649.0);

    testConversion('f32', 'convert_u', 'i64', 1, 1.0);
    testConversion('f32', 'convert_u', 'i64', 0, 0.0);
    testConversion('f32', 'convert_u', 'i64', "0x7fffffffffffffff", 9223372036854775807.0);
    testConversion('f32', 'convert_u', 'i64', "0x8000000000000000", 9223372036854775808.0);
    testConversion('f32', 'convert_u', 'i64', -1, 18446744073709551616.0);
    testConversion('f32', 'convert_u', 'i64', "0xffff0000ffff0000", 18446462598732840000.0);

    testConversion('f64', 'convert_u', 'i64', 1, 1.0);
    testConversion('f64', 'convert_u', 'i64', 0, 0.0);
    testConversion('f64', 'convert_u', 'i64', "0x7fffffffffffffff", 9223372036854775807.0);
    testConversion('f64', 'convert_u', 'i64', "0x8000000000000000", 9223372036854775808.0);
    testConversion('f64', 'convert_u', 'i64', -1, 18446744073709551616.0);
    testConversion('f64', 'convert_u', 'i64', "0xffff0000ffff0000", 18446462603027743000.0);

    testConversion('i64', 'trunc_s', 'f64', 0.0, 0);
    testConversion('i64', 'trunc_s', 'f64', "-0.0", 0);
    testConversion('i64', 'trunc_s', 'f64', 1.0, 1);
    testConversion('i64', 'trunc_s', 'f64', 1.1, 1);
    testConversion('i64', 'trunc_s', 'f64', 1.5, 1);
    testConversion('i64', 'trunc_s', 'f64', 1.99, 1);
    testConversion('i64', 'trunc_s', 'f64', 40.1, 40);
    testConversion('i64', 'trunc_s', 'f64', -1.0, -1);
    testConversion('i64', 'trunc_s', 'f64', -1.1, -1);
    testConversion('i64', 'trunc_s', 'f64', -1.5, -1);
    testConversion('i64', 'trunc_s', 'f64', -1.99, -1);
    testConversion('i64', 'trunc_s', 'f64', -2.0, -2);
    testConversion('i64', 'trunc_s', 'f64', 4294967296.1, "0x100000000");
    testConversion('i64', 'trunc_s', 'f64', -4294967296.8, "0xffffffff00000000");
    testConversion('i64', 'trunc_s', 'f64', 9223372036854774784.8, "0x7ffffffffffffc00");
    testConversion('i64', 'trunc_s', 'f64', -9223372036854775808.3, "0x8000000000000000");

    testConversion('i64', 'trunc_u', 'f64', 0.0, 0);
    testConversion('i64', 'trunc_u', 'f64', "-0.0", 0);
    testConversion('i64', 'trunc_u', 'f64', 1.0, 1);
    testConversion('i64', 'trunc_u', 'f64', 1.1, 1);
    testConversion('i64', 'trunc_u', 'f64', 1.5, 1);
    testConversion('i64', 'trunc_u', 'f64', 1.99, 1);
    testConversion('i64', 'trunc_u', 'f64', -0.9, 0);
    testConversion('i64', 'trunc_u', 'f64', 40.1, 40);
    testConversion('i64', 'trunc_u', 'f64', 4294967295, "0xffffffff");
    testConversion('i64', 'trunc_u', 'f64', 4294967296.1, "0x100000000");
    testConversion('i64', 'trunc_u', 'f64', 1e8, "0x5f5e100");
    testConversion('i64', 'trunc_u', 'f64', 1e16, "0x2386f26fc10000");
    testConversion('i64', 'trunc_u', 'f64', 9223372036854775808, "0x8000000000000000");
    testConversion('i64', 'trunc_u', 'f64', 18446744073709549568.1, -2048);

    testConversion('i64', 'trunc_s', 'f32', 0.0, 0);
    testConversion('i64', 'trunc_s', 'f32', "-0.0", 0);
    testConversion('i64', 'trunc_s', 'f32', 1.0, 1);
    testConversion('i64', 'trunc_s', 'f32', 1.1, 1);
    testConversion('i64', 'trunc_s', 'f32', 1.5, 1);
    testConversion('i64', 'trunc_s', 'f32', 1.99, 1);
    testConversion('i64', 'trunc_s', 'f32', 40.1, 40);
    testConversion('i64', 'trunc_s', 'f32', -1.0, -1);
    testConversion('i64', 'trunc_s', 'f32', -1.1, -1);
    testConversion('i64', 'trunc_s', 'f32', -1.5, -1);
    testConversion('i64', 'trunc_s', 'f32', -1.99, -1);
    testConversion('i64', 'trunc_s', 'f32', -2.0, -2);
    testConversion('i64', 'trunc_s', 'f32', 4294967296.1, "0x100000000");
    testConversion('i64', 'trunc_s', 'f32', -4294967296.8, "0xffffffff00000000");
    testConversion('i64', 'trunc_s', 'f32', 9223371487098961920.0, "0x7fffff8000000000");
    testConversion('i64', 'trunc_s', 'f32', -9223372036854775808.3, "0x8000000000000000");

    testConversion('i64', 'trunc_u', 'f32', 0.0, 0);
    testConversion('i64', 'trunc_u', 'f32', "-0.0", 0);
    testConversion('i64', 'trunc_u', 'f32', 1.0, 1);
    testConversion('i64', 'trunc_u', 'f32', 1.1, 1);
    testConversion('i64', 'trunc_u', 'f32', 1.5, 1);
    testConversion('i64', 'trunc_u', 'f32', 1.99, 1);
    testConversion('i64', 'trunc_u', 'f32', -0.9, 0);
    testConversion('i64', 'trunc_u', 'f32', 40.1, 40);
    testConversion('i64', 'trunc_u', 'f32', 1e8, "0x5f5e100");
    testConversion('i64', 'trunc_u', 'f32', 4294967296, "0x100000000");
    testConversion('i64', 'trunc_u', 'f32', 18446742974197923840.0, "0xffffff0000000000");

    testTrap('i64', 'trunc_s', 'f64', 9223372036854776000.0);
    testTrap('i64', 'trunc_s', 'f64', -9223372036854778000.0);
    testTrap('i64', 'trunc_s', 'f64', "nan");
    testTrap('i64', 'trunc_s', 'f64', "infinity");
    testTrap('i64', 'trunc_s', 'f64', "-infinity");

    testTrap('i64', 'trunc_u', 'f64', -1);
    testTrap('i64', 'trunc_u', 'f64', 18446744073709551616.0);
    testTrap('i64', 'trunc_u', 'f64', "nan");
    testTrap('i64', 'trunc_u', 'f64', "infinity");
    testTrap('i64', 'trunc_u', 'f64', "-infinity");

    testTrap('i64', 'trunc_s', 'f32', 9223372036854776000.0);
    testTrap('i64', 'trunc_s', 'f32', -9223372586610630000.0);
    testTrap('i64', 'trunc_s', 'f32', "nan");
    testTrap('i64', 'trunc_s', 'f32', "infinity");
    testTrap('i64', 'trunc_s', 'f32', "-infinity");

    testTrap('i64', 'trunc_u', 'f32', 18446744073709551616.0);
    testTrap('i64', 'trunc_u', 'f32', -1);
    testTrap('i64', 'trunc_u', 'f32', "nan");
    testTrap('i64', 'trunc_u', 'f32', "infinity");
    testTrap('i64', 'trunc_u', 'f32', "-infinity");

    testConversion('i64', 'reinterpret', 'f64', 40.09999999999968, "0x40440ccccccccca0");
    testConversion('f64', 'reinterpret', 'i64', "0x40440ccccccccca0", 40.09999999999968);

    setJitCompilerOption('wasm.test-mode', 0);
}

// i32.trunc_s* : all values in ] -2**31 - 1; 2**31 [ are acceptable.
// f32:
var p = Math.pow;
testConversion('i32', 'trunc_s', 'f32', 40.1, 40);
testConversion('i32', 'trunc_s', 'f32', p(2, 31) - 128, p(2, 31) - 128); // last f32 value exactly representable < 2**31.
testConversion('i32', 'trunc_s', 'f32', -p(2, 31), -p(2,31)); // last f32 value exactly representable > -2**31 - 1.

testTrap('i32', 'trunc_s', 'f32', 'nan');
testTrap('i32', 'trunc_s', 'f32', 'infinity');
testTrap('i32', 'trunc_s', 'f32', '-infinity');
testTrap('i32', 'trunc_s', 'f32', p(2, 31));
testTrap('i32', 'trunc_s', 'f32', -p(2,31) - 256);

testConversion('i32', 'trunc_s', 'f64', 40.1, 40);
testConversion('i32', 'trunc_s', 'f64', p(2,31) - 0.001, p(2,31) - 1); // example value near the top.
testConversion('i32', 'trunc_s', 'f64', -p(2,31) - 0.999, -p(2,31)); // example value near the bottom.

// f64:
testTrap('i32', 'trunc_s', 'f64', 'nan');
testTrap('i32', 'trunc_s', 'f64', 'infinity');
testTrap('i32', 'trunc_s', 'f64', '-infinity');
testTrap('i32', 'trunc_s', 'f64', p(2,31));
testTrap('i32', 'trunc_s', 'f64', -p(2,31) - 1);

// i32.trunc_u* : all values in ] -1; 2**32 [ are acceptable.
// f32:
testConversion('i32', 'trunc_u', 'f32', 40.1, 40);
testConversion('i32', 'trunc_u', 'f32', p(2,31), p(2,31)|0);
testConversion('i32', 'trunc_u', 'f32', p(2,32) - 256, (p(2,32) - 256)|0); // last f32 value exactly representable < 2**32.
testConversion('i32', 'trunc_u', 'f32', -0.99, 0); // example value near the bottom.

testTrap('i32', 'trunc_u', 'f32', 'nan');
testTrap('i32', 'trunc_u', 'f32', 'infinity');
testTrap('i32', 'trunc_u', 'f32', '-infinity');
testTrap('i32', 'trunc_u', 'f32', -1);
testTrap('i32', 'trunc_u', 'f32', p(2,32));

// f64:
testConversion('i32', 'trunc_u', 'f64', 40.1, 40);
testConversion('i32', 'trunc_u', 'f64', p(2,32) - 0.001, (p(2,32) - 1)|0); // example value near the top.
testConversion('i32', 'trunc_u', 'f64', -0.99999, 0); // example value near the bottom.

testTrap('i32', 'trunc_u', 'f32', 'nan');
testTrap('i32', 'trunc_u', 'f32', 'infinity');
testTrap('i32', 'trunc_u', 'f32', '-infinity');
testTrap('i32', 'trunc_u', 'f32', -1);
testTrap('i32', 'trunc_u', 'f32', p(2,32));

// Other opcodes.
testConversion('i32', 'reinterpret', 'f32', 40.1, 1109419622);
testConversion('f32', 'reinterpret', 'i32', 40, 5.605193857299268e-44);

testConversion('f32', 'convert_s', 'i32', 40, 40);
testConversion('f32', 'convert_u', 'i32', 40, 40);

testConversion('f64', 'convert_s', 'i32', 40, 40);
testConversion('f64', 'convert_u', 'i32', 40, 40);

testConversion('f32', 'demote', 'f64', 40.1, 40.099998474121094);
testConversion('f64', 'promote', 'f32', 40.1, 40.099998474121094);

// Non-canonical NaNs.
assertEq(wasmEvalText('(module (func (result i32) (i32.reinterpret/f32 (f32.demote/f64 (f64.const -nan:0x4444444444444)))) (export "" 0))').exports[""](), -0x1dddde);
assertEq(wasmEvalText('(module (func (result i32) (local i64) (set_local 0 (i64.reinterpret/f64 (f64.promote/f32 (f32.const -nan:0x222222)))) (i32.xor (i32.wrap/i64 (get_local 0)) (i32.wrap/i64 (i64.shr_u (get_local 0) (i64.const 32))))) (export "" 0))').exports[""](), -0x4003bbbc);
