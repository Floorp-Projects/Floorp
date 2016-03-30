load(libdir + "wasm.js");

if (!wasmIsSupported())
    quit();

function testConversion(resultType, opcode, paramType, op, expect) {
  if (paramType === 'i64') {
    // i64 cannot be imported, so we use a wrapper function.
    assertEq(wasmEvalText(`(module
                            (func (param i64) (result ${resultType}) (${resultType}.${opcode}/i64 (get_local 0)))
                            (func (result ${resultType}) (call 0 (i64.const ${op})))
                            (export "" 1))`)(), expect);
    // The same, but now the input is a constant.
    assertEq(wasmEvalText(`(module
                            (func (result ${resultType}) (${resultType}.${opcode}/i64 (i64.const ${op})))
                            (export "" 0))`)(), expect);
  } else if (resultType === 'i64') {
    assertEq(wasmEvalText(`(module
                            (func (param ${paramType}) (result i64) (i64.${opcode}/${paramType} (get_local 0)))
                            (func (result i32) (i64.eq (i64.const ${expect}) (call 0 (${paramType}.const ${op}))))
                            (export "" 1))`)(), 1);
    // The same, but now the input is a constant.
    assertEq(wasmEvalText(`(module
                            (func (result i64) (i64.${opcode}/${paramType} (${paramType}.const ${op})))
                            (func (result i32) (i64.eq (i64.const ${expect}) (call 0)))
                            (export "" 1))`)(), 1);
  } else {
    assertEq(wasmEvalText('(module (func (param ' + paramType + ') (result ' + resultType + ') (' + resultType + '.' + opcode + '/' + paramType + ' (get_local 0))) (export "" 0))')(op), expect);
  }

  // TODO: i64 NYI
  for (var bad of ['i32', 'f32', 'f64']) {
    if (bad != resultType)
      assertErrorMessage(() => wasmEvalText('(module (func (param ' + paramType + ') (result ' + bad + ') (' + resultType + '.' + opcode + '/' + paramType + ' (get_local 0))))'),
                         TypeError,
                         mismatchError(resultType, bad)
                        );
    if (bad != paramType)
      assertErrorMessage(() => wasmEvalText('(module (func (param ' + bad + ') (result ' + resultType + ') (' + resultType + '.' + opcode + '/' + paramType + ' (get_local 0))))'),
                         TypeError,
                         mismatchError(bad, paramType)
                        );
  }
}

if (getBuildConfiguration().x64) {
    testConversion('i32', 'wrap', 'i64', 4294967336, 40);
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
    testConversion('f32', 'convert_s', 'i64', "9223372036854775807", 9223372036854775807.0);
    testConversion('f32', 'convert_s', 'i64', "-9223372036854775808", -9223372036854775808.0);
    testConversion('f32', 'convert_s', 'i64', "314159265358979", 314159275180032.0);

    testConversion('f64', 'convert_s', 'i64', 1, 1.0);
    testConversion('f64', 'convert_s', 'i64', -1, -1.0);
    testConversion('f64', 'convert_s', 'i64', 0, 0.0);
    testConversion('f64', 'convert_s', 'i64', "9223372036854775807", 9223372036854775807.0);
    testConversion('f64', 'convert_s', 'i64', "-9223372036854775808", -9223372036854775808.0);
    testConversion('f64', 'convert_s', 'i64', "4669201609102990", 4669201609102990);

    testConversion('f32', 'convert_u', 'i64', 1, 1.0);
    testConversion('f32', 'convert_u', 'i64', 0, 0.0);
    testConversion('f32', 'convert_u', 'i64', "9223372036854775807", 9223372036854775807.0);
    testConversion('f32', 'convert_u', 'i64', "-9223372036854775808", 9223372036854775808.0);
    testConversion('f32', 'convert_u', 'i64', -1, 18446744073709551616.0);
    testConversion('f32', 'convert_u', 'i64', "0xffff0000ffff0000", 18446462598732840000.0);

    testConversion('f64', 'convert_u', 'i64', 1, 1.0);
    testConversion('f64', 'convert_u', 'i64', 0, 0.0);
    testConversion('f64', 'convert_u', 'i64', "9223372036854775807", 9223372036854775807.0);
    testConversion('f64', 'convert_u', 'i64', "-9223372036854775808", 9223372036854775808.0);
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
    testConversion('i64', 'trunc_s', 'f64', 4294967296.1, "4294967296");
    testConversion('i64', 'trunc_s', 'f64', -4294967296.8, "-4294967296");
    testConversion('i64', 'trunc_s', 'f64', 9223372036854774784.8, "9223372036854774784");
    testConversion('i64', 'trunc_s', 'f64', -9223372036854775808.3, "-9223372036854775808");

    testConversion('i64', 'trunc_u', 'f64', 0.0, 0);
    testConversion('i64', 'trunc_u', 'f64', "-0.0", 0);
    testConversion('i64', 'trunc_u', 'f64', 1.0, 1);
    testConversion('i64', 'trunc_u', 'f64', 1.1, 1);
    testConversion('i64', 'trunc_u', 'f64', 1.5, 1);
    testConversion('i64', 'trunc_u', 'f64', 1.99, 1);
    testConversion('i64', 'trunc_u', 'f64', -0.9, 0);
    testConversion('i64', 'trunc_u', 'f64', 40.1, 40);
    testConversion('i64', 'trunc_u', 'f64', 4294967295, "0xffffffff");
    testConversion('i64', 'trunc_u', 'f64', 4294967296.1, "4294967296");
    testConversion('i64', 'trunc_u', 'f64', 1e8, "100000000");
    testConversion('i64', 'trunc_u', 'f64', 1e16, "10000000000000000");
    testConversion('i64', 'trunc_u', 'f64', 9223372036854775808, "-9223372036854775808");
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
    testConversion('i64', 'trunc_s', 'f32', 4294967296.1, "4294967296");
    testConversion('i64', 'trunc_s', 'f32', -4294967296.8, "-4294967296");
    testConversion('i64', 'trunc_s', 'f32', 9223371487098961920.0, "9223371487098961920");
    testConversion('i64', 'trunc_s', 'f32', -9223372036854775808.3, "-9223372036854775808");

    testConversion('i64', 'trunc_u', 'f32', 0.0, 0);
    testConversion('i64', 'trunc_u', 'f32', "-0.0", 0);
    testConversion('i64', 'trunc_u', 'f32', 1.0, 1);
    testConversion('i64', 'trunc_u', 'f32', 1.1, 1);
    testConversion('i64', 'trunc_u', 'f32', 1.5, 1);
    testConversion('i64', 'trunc_u', 'f32', 1.99, 1);
    testConversion('i64', 'trunc_u', 'f32', -0.9, 0);
    testConversion('i64', 'trunc_u', 'f32', 40.1, 40);
    testConversion('i64', 'trunc_u', 'f32', 1e8, "100000000");
    testConversion('i64', 'trunc_u', 'f32', 4294967296, "4294967296");
    testConversion('i64', 'trunc_u', 'f32', 18446742974197923840.0, "-1099511627776");

    // TODO: these should trap.
    testConversion('i64', 'trunc_s', 'f64', 9223372036854775808.0, "0x8000000000000000");
    testConversion('i64', 'trunc_s', 'f64', -9223372036854777856.0, "0x8000000000000000");
    testConversion('i64', 'trunc_s', 'f64', "nan", "0x8000000000000000");
    testConversion('i64', 'trunc_s', 'f64', "infinity", "0x8000000000000000");
    testConversion('i64', 'trunc_s', 'f64', "-infinity", "0x8000000000000000");

    testConversion('i64', 'trunc_u', 'f64', -1, "0x8000000000000000");
    testConversion('i64', 'trunc_u', 'f64', 18446744073709551616.0, "0x8000000000000000");
    testConversion('i64', 'trunc_u', 'f64', "nan", "0x8000000000000000");
    testConversion('i64', 'trunc_u', 'f64', "infinity", "0x8000000000000000");
    testConversion('i64', 'trunc_u', 'f64', "-infinity", "0x8000000000000000");

    testConversion('i64', 'trunc_s', 'f32', 9223372036854775808.0, "0x8000000000000000");
    testConversion('i64', 'trunc_s', 'f32', -9223372036854777856.0, "0x8000000000000000");
    testConversion('i64', 'trunc_s', 'f32', "nan", "0x8000000000000000");
    testConversion('i64', 'trunc_s', 'f32', "infinity", "0x8000000000000000");
    testConversion('i64', 'trunc_s', 'f32', "-infinity", "0x8000000000000000");

    testConversion('i64', 'trunc_u', 'f32', 18446744073709551616.0, "0x8000000000000000");
    testConversion('i64', 'trunc_u', 'f32', -1, "0x8000000000000000");
    testConversion('i64', 'trunc_u', 'f32', "nan", "0x8000000000000000");
    testConversion('i64', 'trunc_u', 'f32', "infinity", "0x8000000000000000");
    testConversion('i64', 'trunc_u', 'f32', "-infinity", "0x8000000000000000");

    testConversion('i64', 'reinterpret', 'f64', 40.09999999999968, 4630840390592548000);
    testConversion('f64', 'reinterpret', 'i64', 4630840390592548000, 40.09999999999968);
} else {
    // Sleeper test: once i64 works on more platforms, remove this if-else.
    try {
        testConversion('i32', 'wrap', 'i64', 4294967336, 40);
        assertEq(0, 1);
    } catch(e) {
        assertEq(e.toString().indexOf("NYI on this platform") >= 0, true);
    }
}

testConversion('i32', 'trunc_s', 'f32', 40.1, 40);
testConversion('i32', 'trunc_u', 'f32', 40.1, 40);
testConversion('i32', 'trunc_s', 'f64', 40.1, 40);
testConversion('i32', 'trunc_u', 'f64', 40.1, 40);
testConversion('i32', 'reinterpret', 'f32', 40.1, 1109419622);

testConversion('f32', 'convert_s', 'i32', 40, 40);
testConversion('f32', 'convert_u', 'i32', 40, 40);
testConversion('f32', 'demote', 'f64', 40.1, 40.099998474121094);
testConversion('f32', 'reinterpret', 'i32', 40, 5.605193857299268e-44);

testConversion('f64', 'convert_s', 'i32', 40, 40);
testConversion('f64', 'convert_u', 'i32', 40, 40);
testConversion('f64', 'promote', 'f32', 40.1, 40.099998474121094);
