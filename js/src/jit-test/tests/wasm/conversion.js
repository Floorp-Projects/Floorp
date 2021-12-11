function testConversion0(resultType, opcode, paramType, op, expect) {
    if (resultType === 'i64') {
        wasmFullPassI64(`(module
            (func $run (param ${paramType}) (result ${resultType})
                (${opcode} (local.get 0))
            )
        )`, expect, {}, `${paramType}.const ${op}`);

        // The same, but now the input is a constant.
        wasmFullPassI64(`(module
            (func $run (result ${resultType})
                (${opcode} (${paramType}.const ${op}))
            )
        )`, expect);
    } else if (paramType === 'i64') {
        wasmFullPass(`(module
           (func $f (param ${paramType}) (result ${resultType})
            (${opcode} (local.get 0))
           )
           (func (export "run") (result ${resultType})
            i64.const ${op}
            call $f
           )
        )`, expect, {});
    } else {
        wasmFullPass(`(module
           (func (param ${paramType}) (result ${resultType})
            (${opcode} (local.get 0)))
            (export "run" (func 0))
        )`, expect, {}, op);
    }

    for (var bad of ['i32', 'f32', 'f64', 'i64']) {
        if (bad !== resultType) {
            wasmFailValidateText(
                `(module (func (param ${paramType}) (result ${bad}) (${opcode} (local.get 0))))`,
                mismatchError(resultType, bad)
            );
        }

        if (bad !== paramType) {
            wasmFailValidateText(
                `(module (func (param ${bad}) (result ${resultType}) (${opcode} (local.get 0))))`,
                mismatchError(bad, paramType)
            );
        }
    }
}

function testConversion(resultType, opcode, paramType, op, expect) {
  testConversion0(resultType, `${resultType}.${opcode}/${paramType}`, paramType, op, expect);
}

function testSignExtension(resultType, opcode, paramType, op, expect) {
  testConversion0(resultType, `${resultType}.${opcode}`, paramType, op, expect);
}

function testTrap(resultType, opcode, paramType, op) {
    let func = wasmEvalText(`(module
        (func
            (param ${paramType})
            (result ${resultType})
            (${resultType}.${opcode}/${paramType} (local.get 0))
        )
        (func
            (param ${paramType})
            local.get 0
            call 0
            drop
        )
        (export "" (func 1))
    )`).exports[""];

    let expectedError = op === 'nan' ? /invalid conversion to integer/ : /integer overflow/;

    assertErrorMessage(() => func(jsify(op)), Error, expectedError);
}

var p = Math.pow;

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
testConversion('f32', 'convert_s', 'i64', "0x7fffffff", 2147483648.0); // closest approx.
testConversion('f32', 'convert_s', 'i64', "0x80000000", 2147483648.0);
testConversion('f32', 'convert_s', 'i64', "0x80000001", 2147483648.0); // closest approx.

// Interesting values at the boundaries.
testConversion('f32', 'convert_s', 'i64', "0x358a09a000000002", 3857906751034621952);
testConversion('f32', 'convert_s', 'i64', "0x8000004000000001", -9223371487098961920);
testConversion('f32', 'convert_s', 'i64', "0xffdfffffdfffffff", -9007200328482816);
testConversion('f32', 'convert_s', 'i64', "0x0020000020000001", 9007200328482816);
testConversion('f32', 'convert_s', 'i64', "0x7fffff4000000001", 9223371487098961920);

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

// Interesting values at the boundaries.
testConversion('f32', 'convert_u', 'i64', "0x100404900000008", 72128280609685500);
testConversion('f32', 'convert_u', 'i64', "0x7fffff4000000001", 9223371487098962000);
testConversion('f32', 'convert_u', 'i64', "0x0020000020000001", 9007200328482816);
testConversion('f32', 'convert_u', 'i64', "0x7fffffbfffffffff", 9223371487098961920);
testConversion('f32', 'convert_u', 'i64', "0x8000008000000001", 9223373136366403584);
testConversion('f32', 'convert_u', 'i64', "0xfffffe8000000001", 18446742974197923840);

testConversion('f64', 'convert_u', 'i64', 1, 1.0);
testConversion('f64', 'convert_u', 'i64', 0, 0.0);
testConversion('f64', 'convert_u', 'i64', "0x7fffffffffffffff", 9223372036854775807.0);
testConversion('f64', 'convert_u', 'i64', "0x8000000000000000", 9223372036854775808.0);
testConversion('f64', 'convert_u', 'i64', -1, 18446744073709551616.0);
testConversion('f64', 'convert_u', 'i64', "0xffff0000ffff0000", 18446462603027743000.0);
testConversion('f64', 'convert_u', 'i64', "0xbf869c3369c26401", 13800889852755077000);
testConversion('f64', 'convert_u', 'i64', "0x7fffff4000000001", 9223371212221054976);
testConversion('f64', 'convert_u', 'i64', "0x8000008000000001", 9223372586610589696);
testConversion('f64', 'convert_u', 'i64', "0xfffffe8000000001", 18446742424442109952);

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
testTrap('i64', 'trunc_s', 'f64', "inf");
testTrap('i64', 'trunc_s', 'f64', "-inf");

testTrap('i64', 'trunc_u', 'f64', -1);
testTrap('i64', 'trunc_u', 'f64', 18446744073709551616.0);
testTrap('i64', 'trunc_u', 'f64', "nan");
testTrap('i64', 'trunc_u', 'f64', "inf");
testTrap('i64', 'trunc_u', 'f64', "-inf");

testTrap('i64', 'trunc_s', 'f32', 9223372036854776000.0);
testTrap('i64', 'trunc_s', 'f32', -9223372586610630000.0);
testTrap('i64', 'trunc_s', 'f32', "nan");
testTrap('i64', 'trunc_s', 'f32', "inf");
testTrap('i64', 'trunc_s', 'f32', "-inf");

testTrap('i64', 'trunc_u', 'f32', 18446744073709551616.0);
testTrap('i64', 'trunc_u', 'f32', -1);
testTrap('i64', 'trunc_u', 'f32', "nan");
testTrap('i64', 'trunc_u', 'f32', "inf");
testTrap('i64', 'trunc_u', 'f32', "-inf");

testConversion('i64', 'reinterpret', 'f64', 40.09999999999968, "0x40440ccccccccca0");
testConversion('f64', 'reinterpret', 'i64', "0x40440ccccccccca0", 40.09999999999968);

var u64max = '0xffffffffffffffff';
var s64max = '0x7fffffffffffffff';
var s64min = '-0x8000000000000000';
var s32max = 2147483647;
var s32min = -2147483648;

testConversion('i32', 'trunc_s:sat', 'f32', NaN, 0);
testConversion('i32', 'trunc_s:sat', 'f32', Infinity, s32max);
testConversion('i32', 'trunc_s:sat', 'f32', -Infinity, s32min);
testConversion('i32', 'trunc_s:sat', 'f32', p(2, 31), s32max);
testConversion('i32', 'trunc_s:sat', 'f32', -p(2, 31) - 256, s32min);

testConversion('i32', 'trunc_s:sat', 'f64', NaN, 0);
testConversion('i32', 'trunc_s:sat', 'f64', Infinity, s32max);
testConversion('i32', 'trunc_s:sat', 'f64', -Infinity, s32min);
testConversion('i32', 'trunc_s:sat', 'f64', p(2, 31), s32max);
testConversion('i32', 'trunc_s:sat', 'f64', -p(2, 31) - 1, s32min);

testConversion('i32', 'trunc_u:sat', 'f32', NaN, 0);
testConversion('i32', 'trunc_u:sat', 'f32', Infinity, -1);
testConversion('i32', 'trunc_u:sat', 'f32', -Infinity, 0);
testConversion('i32', 'trunc_u:sat', 'f32', -1, 0);
testConversion('i32', 'trunc_u:sat', 'f32', p(2, 32), -1);

testConversion('i32', 'trunc_u:sat', 'f64', NaN, 0);
testConversion('i32', 'trunc_u:sat', 'f64', Infinity, -1);
testConversion('i32', 'trunc_u:sat', 'f64', -Infinity, 0);
testConversion('i32', 'trunc_u:sat', 'f64', -1, 0);
testConversion('i32', 'trunc_u:sat', 'f64', p(2, 32), -1);

testConversion('i64', 'trunc_s:sat', 'f64', 9223372036854776000.0, s64max);
testConversion('i64', 'trunc_s:sat', 'f64', -9223372036854778000.0, s64min);
testConversion('i64', 'trunc_s:sat', 'f64', 'nan', '0');
testConversion('i64', 'trunc_s:sat', 'f64', 'inf', s64max);
testConversion('i64', 'trunc_s:sat', 'f64', '-inf', s64min);

testConversion('i64', 'trunc_u:sat', 'f64', -1, '0');
testConversion('i64', 'trunc_u:sat', 'f64', 18446744073709551616.0, u64max);
testConversion('i64', 'trunc_u:sat', 'f64', 'nan', '0');
testConversion('i64', 'trunc_u:sat', 'f64', 'inf', u64max);
testConversion('i64', 'trunc_u:sat', 'f64', '-inf', '0');

testConversion('i64', 'trunc_s:sat', 'f32', 9223372036854776000.0, s64max);
testConversion('i64', 'trunc_s:sat', 'f32', -9223372586610630000.0, s64min);
testConversion('i64', 'trunc_s:sat', 'f32', 'nan', '0');
testConversion('i64', 'trunc_s:sat', 'f32', 'inf', s64max);
testConversion('i64', 'trunc_s:sat', 'f32', '-inf', s64min);

testConversion('i64', 'trunc_u:sat', 'f32', 18446744073709551616.0, u64max);
testConversion('i64', 'trunc_u:sat', 'f32', -1, '0');
testConversion('i64', 'trunc_u:sat', 'f32', 'nan', '0');
testConversion('i64', 'trunc_u:sat', 'f32', 'inf', u64max);
testConversion('i64', 'trunc_u:sat', 'f32', '-inf', '0');

testSignExtension('i32', 'extend8_s', 'i32', 0x7F, 0x7F);
testSignExtension('i32', 'extend8_s', 'i32', 0x80, -0x80);
testSignExtension('i32', 'extend16_s', 'i32', 0x7FFF, 0x7FFF);
testSignExtension('i32', 'extend16_s', 'i32', 0x8000, -0x8000);
testSignExtension('i64', 'extend8_s', 'i64', 0x7F, 0x7F);
testSignExtension('i64', 'extend8_s', 'i64', 0x80, -0x80);
testSignExtension('i64', 'extend16_s', 'i64', 0x7FFF, 0x7FFF);
testSignExtension('i64', 'extend16_s', 'i64', 0x8000, -0x8000);
testSignExtension('i64', 'extend32_s', 'i64', 0x7FFFFFFF, 0x7FFFFFFF);
testSignExtension('i64', 'extend32_s', 'i64', "0x80000000", "0xFFFFFFFF80000000");

// i32.trunc_s* : all values in ] -2**31 - 1; 2**31 [ are acceptable.
// f32:
testConversion('i32', 'trunc_s', 'f32', 40.1, 40);
testConversion('i32', 'trunc_s', 'f32', p(2, 31) - 128, p(2, 31) - 128); // last f32 value exactly representable < 2**31.
testConversion('i32', 'trunc_s', 'f32', -p(2, 31), -p(2,31)); // last f32 value exactly representable > -2**31 - 1.

testTrap('i32', 'trunc_s', 'f32', 'nan');
testTrap('i32', 'trunc_s', 'f32', 'inf');
testTrap('i32', 'trunc_s', 'f32', '-inf');
testTrap('i32', 'trunc_s', 'f32', p(2, 31));
testTrap('i32', 'trunc_s', 'f32', -p(2,31) - 256);

testConversion('i32', 'trunc_s', 'f64', 40.1, 40);
testConversion('i32', 'trunc_s', 'f64', p(2,31) - 0.001, p(2,31) - 1); // example value near the top.
testConversion('i32', 'trunc_s', 'f64', -p(2,31) - 0.999, -p(2,31)); // example value near the bottom.

// f64:
testTrap('i32', 'trunc_s', 'f64', 'nan');
testTrap('i32', 'trunc_s', 'f64', 'inf');
testTrap('i32', 'trunc_s', 'f64', '-inf');
testTrap('i32', 'trunc_s', 'f64', p(2,31));
testTrap('i32', 'trunc_s', 'f64', -p(2,31) - 1);

// i32.trunc_u* : all values in ] -1; 2**32 [ are acceptable.
// f32:
testConversion('i32', 'trunc_u', 'f32', 40.1, 40);
testConversion('i32', 'trunc_u', 'f32', p(2,31), p(2,31)|0);
testConversion('i32', 'trunc_u', 'f32', p(2,32) - 256, (p(2,32) - 256)|0); // last f32 value exactly representable < 2**32.
testConversion('i32', 'trunc_u', 'f32', -0.99, 0); // example value near the bottom.

testTrap('i32', 'trunc_u', 'f32', 'nan');
testTrap('i32', 'trunc_u', 'f32', 'inf');
testTrap('i32', 'trunc_u', 'f32', '-inf');
testTrap('i32', 'trunc_u', 'f32', -1);
testTrap('i32', 'trunc_u', 'f32', p(2,32));

// f64:
testConversion('i32', 'trunc_u', 'f64', 40.1, 40);
testConversion('i32', 'trunc_u', 'f64', p(2,32) - 0.001, (p(2,32) - 1)|0); // example value near the top.
testConversion('i32', 'trunc_u', 'f64', -0.99999, 0); // example value near the bottom.

testTrap('i32', 'trunc_u', 'f64', 'nan');
testTrap('i32', 'trunc_u', 'f64', 'inf');
testTrap('i32', 'trunc_u', 'f64', '-inf');
testTrap('i32', 'trunc_u', 'f64', -1);
testTrap('i32', 'trunc_u', 'f64', p(2,32));

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

// Wasm v1.1 spec 4.3.4 "Conversions", subsection demote(z): if z is a NaN but
// not canonical, return any NaN.  That is, the sign is arbitrary and the
// payload is arbitrary, except that the most significant bit of the payload
// must be set.  We check simply that a quiet NaN is returned and ignore the
// sign and payload.

wasmFullPass(`
(module
 (func (result i32)
  (i32.and
    (i32.const 0x7FC00000)
    (i32.reinterpret/f32
      (f32.demote/f64 (f64.const -nan:0x4444444444444)))))
 (export "run" (func 0)))`,
             0x7FC00000);

// Wasm v1.1 spec 4.3.4 "Conversions", subsection promote(z): if z is a NaN but
// not canonical, return any NaN.  See above.

wasmFullPass(`
(module
 (func (result i64)
   (i64.and
     (i64.const 0x7FF8000000000000)
     (i64.reinterpret/f64 (f64.promote/f32 (f32.const -nan:0x222222)))))
 (export "run" (func 0)))`,
             0x7FF8_0000_0000_0000n);
