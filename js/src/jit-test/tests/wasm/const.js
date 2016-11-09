load(libdir + "wasm.js");

function testConst(type, str, expect) {
    if (type === 'i64')
        wasmFullPassI64(`(module (func (result i64) (i64.const ${str})) (export "run" 0))`, expect);
    else
        wasmFullPass(`(module (func (result ${type}) (${type}.const ${str})) (export "run" 0))`, expect);
}

function testConstError(type, str) {
  // For now at least, we don't distinguish between parse errors and OOMs.
  assertErrorMessage(() => wasmEvalText(`(module (func (result ${type}) (${type}.const ${str})) (export "" 0))`).exports[""](), Error, /parsing wasm text/);
}

testConst('i32', '0', 0);
testConst('i32', '-0', 0);
testConst('i32', '23', 23);
testConst('i32', '-23', -23);
testConst('i32', '0x23', 35);
testConst('i32', '-0x23', -35);
testConst('i32', '2147483647', 2147483647);
testConst('i32', '4294967295', -1);
testConst('i32', '-2147483648', -2147483648);
testConst('i32', '0x7fffffff', 2147483647);
testConst('i32', '0x80000000', -2147483648);
testConst('i32', '-0x80000000', -2147483648);
testConst('i32', '0xffffffff', -1);

{
    setJitCompilerOption('wasm.test-mode', 1);

    testConst('i64', '0', 0);
    testConst('i64', '-0', 0);

    testConst('i64', '23', 23);
    testConst('i64', '-23', -23);

    testConst('i64', '0x23', 35);
    testConst('i64', '-0x23', -35);

    testConst('i64', '-0x1', -1);
    testConst('i64', '-1', -1);
    testConst('i64', '0xffffffffffffffff', -1);

    testConst('i64', '0xdeadc0de', {low: 0xdeadc0de, high: 0x0});
    testConst('i64', '0x1337c0de00000000', {low: 0x0, high: 0x1337c0de});

    testConst('i64', '0x0102030405060708', {low: 0x05060708, high: 0x01020304});
    testConst('i64', '-0x0102030405060708', {low: -0x05060708, high: -0x01020305});

    // INT64_MAX
    testConst('i64', '9223372036854775807', {low: 0xffffffff, high: 0x7fffffff});
    testConst('i64', '0x7fffffffffffffff',  {low: 0xffffffff, high: 0x7fffffff});

    // INT64_MAX + 1
    testConst('i64', '9223372036854775808', {low: 0x00000000, high: 0x80000000});
    testConst('i64', '0x8000000000000000', {low: 0x00000000, high: 0x80000000});

    // UINT64_MAX
    testConst('i64', '18446744073709551615', {low: 0xffffffff, high: 0xffffffff});

    // INT64_MIN
    testConst('i64', '-9223372036854775808', {low: 0x00000000, high: 0x80000000});
    testConst('i64', '-0x8000000000000000',  {low: 0x00000000, high: 0x80000000});

    // INT64_MIN - 1
    testConstError('i64', '-9223372036854775809');

    testConstError('i64', '');
    testConstError('i64', '0.0');
    testConstError('i64', 'not an i64');

    setJitCompilerOption('wasm.test-mode', 0);
}

testConst('f32', '0.0', 0.0);
testConst('f32', '-0', -0.0);
testConst('f32', '-0.0', -0.0);
testConst('f32', '0x0.0', 0.0);
testConst('f32', '-0x0.0', -0.0);
testConst('f32', '-0x0', -0.0);
testConst('f32', '0x0.0p0', 0.0);
testConst('f32', '-0x0.0p0', -0.0);
testConst('f32', 'infinity', Infinity);
testConst('f32', '-infinity', -Infinity);
testConst('f32', '+infinity', Infinity);
testConst('f32', 'nan', NaN);
//testConst('f32', '-nan', NaN); // TODO: NYI
testConst('f32', '+nan', NaN);
//testConst('f32', 'nan:0x789', NaN); // TODO: NYI
//testConst('f32', '-nan:0x789', NaN); // TODO: NYI
//testConst('f32', '+nan:0x789', NaN); // TODO: NYI
testConst('f32', '0x01p-149', 1.401298464324817e-45);
testConst('f32', '0x1p-149', 1.401298464324817e-45);
testConst('f32', '0x1p-150', 0);
testConst('f32', '0x2p-150', 1.401298464324817e-45);
testConst('f32', '0x1.2p-149', 1.401298464324817e-45);
testConst('f32', '0x2.0p-149', 2.802596928649634e-45);
testConst('f32', '0x2.2p-149', 2.802596928649634e-45);
testConst('f32', '0x01p-148', 2.802596928649634e-45);
testConst('f32', '0x0.1p-148', 0);
testConst('f32', '0x0.1p-145', 1.401298464324817e-45);
testConst('f32', '0x1p-148', 2.802596928649634e-45);
testConst('f32', '0x1.111p-148', 2.802596928649634e-45);
testConst('f32', '0x1.2p-148', 2.802596928649634e-45);
testConst('f32', '0x2.0p-148', 5.605193857299268e-45);
testConst('f32', '0x2.2p-148', 5.605193857299268e-45);
testConst('f32', '0x1p-147', 5.605193857299268e-45);
testConst('f32', '0x1p-126', 1.1754943508222875e-38);
testConst('f32', '0x0.1fffffep+131', 3.4028234663852886e+38);
testConst('f32', '0x1.fffffep+127', 3.4028234663852886e+38);
testConst('f32', '0x2.0p+127', Infinity);
testConst('f32', '0x1.fffffep+128', Infinity);
testConst('f32', '0x0.1fffffep+128', 4.2535293329816107e+37);
testConst('f32', '0x1p2', 4);
testConst('f32', '0x10p2', 64);
testConst('f32', '0x100p2', 1024);
testConst('f32', '0x2p2', 8);
testConst('f32', '0x4p2', 16);
testConst('f32', '0x1p3', 8);
testConst('f32', '0x1p4', 16);
testConst('f32', '-0x1p+3', -8);
testConst('f32', '0x3p-2', .75);
testConst('f32', '-0x76.54p-32', -2.7550413506105542e-8);
testConst('f32', '0xf.ffffffffffffffffp+123', 170141183460469231731687303715884105728);
testConst('f32', '0xf.ffffffffffffffffp+124', Infinity);
testConst('f32', '1.1754943508222875e-38', 1.1754943508222875e-38);
testConst('f32', '3.4028234663852886e+38', 3.4028234663852886e+38);
testConst('f32', '1.1754943508222875e-35', 1.1754943508222875e-35);
testConst('f32', '3.4028234663852886e+35', 3.4028234346940236e+35);
testConst('f32', '1.1754943508222875e-30', 1.1754943508222875e-30);
testConst('f32', '3.4028234663852886e+30', 3.4028233462973677e+30);
testConst('f32', '4.0', 4);
testConst('f32', '-8.', -8);
testConst('f32', '-2.7550413506105542e-8', -2.7550413506105542e-8);
testConst('f32', '2.138260e+05', 2.138260e+05);
testConst('f32', '3.891074380317903e-33', 3.891074380317903e-33);
testConst('f32', '-9465807272673280.0', -9465807272673280);
testConst('f32', '1076.1376953125', 1076.1376953125);
testConst('f32', '-13364.1376953125', -13364.1376953125);
testConst('f32', '4.133607864379883', 4.133607864379883);
testConst('f32', '2.0791168212890625', 2.0791168212890625);
testConst('f32', '0.000002414453774690628', 0.000002414453774690628);
testConst('f32', '0.5312881469726562', 0.5312881469726562);
testConst('f32', '5.570960e+05', 5.570960e+05);
testConst('f32', '5.066758603788912e-7', 5.066758603788912e-7);
testConst('f32', '-5.066758603788912e-7', -5.066758603788912e-7);
testConst('f32', '1.875000e-01', 1.875000e-01);
testConst('f32', '-0x1.b021fb98e9a17p-104', -8.322574059965897e-32);
testConst('f32', '0x1.08de5bf3f784cp-129', 1.5202715065429227e-39);
testConst('f32', '0x1.d50b969fbbfb3p+388', Infinity);
testConst('f32', '0x3434.2p4', 2.138260e+05);
testConst('f32', '0x1434.2p-120', 3.891074380317903e-33);
testConst('f32', '-0x0434.234p43', -9465807272673280);
testConst('f32', '0x0434.234p0', 1076.1376953125);
testConst('f32', '-0x3434.234p0', -13364.1376953125);
testConst('f32', '0x4.22342p0', 4.133607864379883);
testConst('f32', '0x30000p-20', 1.875000e-01);
testConst('f32', '0x0.533fcccp-125', 7.645233588931088e-39);
testConst('f32', '0', 0);

testConst('f64', '0.0', 0.0);
testConst('f64', '-0.0', -0.0);
testConst('f64', '-0', -0.0);
testConst('f64', '0x0.0', 0.0);
testConst('f64', '-0x0.0', -0.0);
testConst('f64', '-0x0', -0.0);
testConst('f64', '0x0.0p0', 0.0);
testConst('f64', '-0x0.0p0', -0.0);
testConst('f64', 'infinity', Infinity);
testConst('f64', '-infinity', -Infinity);
testConst('f64', '+infinity', Infinity);
testConst('f64', 'nan', NaN);
//testConst('f64', '-nan', NaN); // TODO: NYI
testConst('f64', '+nan', NaN);
//testConst('f64', 'nan:0x789', NaN); // TODO: NYI
//testConst('f64', '-nan:0x789', NaN); // TODO: NYI
//testConst('f64', '+nan:0x789', NaN); // TODO: NYI
testConst('f64', '0x01p-149', 1.401298464324817e-45);
testConst('f64', '0x1p-149', 1.401298464324817e-45);
testConst('f64', '0x1p-150', 7.006492321624085e-46);
testConst('f64', '0x2p-150', 1.401298464324817e-45);
testConst('f64', '0x1.2p-149', 1.5764607723654192e-45);
testConst('f64', '0x2.0p-149', 2.802596928649634e-45);
testConst('f64', '0x2.2p-149', 2.977759236690236e-45);
testConst('f64', '0x01p-148', 2.802596928649634e-45);
testConst('f64', '0x0.1p-148', 1.7516230804060213e-46);
testConst('f64', '0x0.1p-145', 1.401298464324817e-45);
testConst('f64', '0x1p-148', 2.802596928649634e-45);
testConst('f64', '0x1.111p-148', 2.9893911087085575e-45);
testConst('f64', '0x1.2p-148', 3.1529215447308384e-45);
testConst('f64', '0x2.0p-148', 5.605193857299268e-45);
testConst('f64', '0x2.2p-148', 5.955518473380473e-45);
testConst('f64', '0x1p-147', 5.605193857299268e-45);
testConst('f64', '0x1p-126', 1.1754943508222875e-38);
testConst('f64', '0x0.1fffffep+131', 3.4028234663852886e+38);
testConst('f64', '0x1.fffffep+127', 3.4028234663852886e+38);
testConst('f64', '0x2.0p+127', 3.402823669209385e+38);
testConst('f64', '0x1.fffffep+128', 6.805646932770577e+38);
testConst('f64', '0x0.1fffffep+128', 4.2535293329816107e+37);
testConst('f64', '0x1p2', 4);
testConst('f64', '0x10p2', 64);
testConst('f64', '0x100p2', 1024);
testConst('f64', '0x2p2', 8);
testConst('f64', '0x4p2', 16);
testConst('f64', '0x1p3', 8);
testConst('f64', '0x1p4', 16);
testConst('f64', '-0x1p+3', -8);
testConst('f64', '0x3p-2', .75);
testConst('f64', '-0x76.54p-32', -2.7550413506105542e-8);
testConst('f64', '1.1754943508222875e-38', 1.1754943508222875e-38);
testConst('f64', '3.4028234663852886e+38', 3.4028234663852886e+38);
testConst('f64', '1.1754943508222875e-35', 1.1754943508222875e-35);
testConst('f64', '3.4028234663852886e+35', 3.4028234663852886e+35);
testConst('f64', '1.1754943508222875e-30', 1.1754943508222875e-30);
testConst('f64', '3.4028234663852886e+30', 3.402823466385289e+30);
testConst('f64', '4.0', 4);
testConst('f64', '-8.', -8);
testConst('f64', '-2.7550413506105542e-8', -2.7550413506105542e-8);
testConst('f64', '2.138260e+05', 2.138260e+05);
testConst('f64', '3.891074380317903e-33', 3.891074380317903e-33);
testConst('f64', '-9465807272673280.0', -9465807272673280);
testConst('f64', '1076.1376953125', 1076.1376953125);
testConst('f64', '-13364.1376953125', -13364.1376953125);
testConst('f64', '4.133607864379883', 4.133607864379883);
testConst('f64', '2.0791168212890625', 2.0791168212890625);
testConst('f64', '0.000002414453774690628', 0.000002414453774690628);
testConst('f64', '0.5312881469726562', 0.5312881469726562);
testConst('f64', '5.570960e+05', 5.570960e+05);
testConst('f64', '5.066758603788912e-7', 5.066758603788912e-7);
testConst('f64', '-5.066758603788912e-7', -5.066758603788912e-7);
testConst('f64', '1.875000e-01', 1.875000e-01);
testConst('f64', '0x3434.2p4', 2.138260e+05);
testConst('f64', '0x1434.2p-120', 3.891074380317903e-33);
testConst('f64', '-0x0434.234p43', -9465807272673280);
testConst('f64', '0x0434.234p0', 1076.1376953125);
testConst('f64', '-0x3434.234p0', -13364.1376953125);
testConst('f64', '0x4.22342p0', 4.133607864379883);
testConst('f64', '0x4.2882000p-1', 2.0791168212890625);
testConst('f64', '0x30000p-20', 1.875000e-01);
testConst('f64', '0x2f05.000bef2113p-1036', 1.634717678224908e-308);
testConst('f64', '0x24c6.004d0deaa3p-1036', 1.2784941357502007e-308);
testConst('f64', '0', 0);

testConstError('i32', '');
testConstError('i32', '0.0');
testConstError('i32', 'not an i32');
testConstError('i32', '4294967296');
testConstError('i32', '-2147483649');

testConstError('f32', '');
testConstError('f32', 'not an f32');
testConstError('f32', 'nan:');
testConstError('f32', 'nan:0');
testConstError('f32', 'nan:0x');
testConstError('f32', 'nan:0x0');

testConstError('f64', '');
testConstError('f64', 'not an f64');
testConstError('f64', 'nan:');
testConstError('f64', 'nan:0');
testConstError('f64', 'nan:0x');
testConstError('f64', 'nan:0x0');
