load(libdir + "wasm.js");

if (!wasmIsSupported())
    quit();

function mismatchError(actual, expect) {
    var str = "type mismatch: expression has type " + actual + " but expected " + expect;
    return RegExp(str);
}

function testConversion(resultType, opcode, paramType, op, expect) {
  assertEq(wasmEvalText('(module (func (param ' + paramType + ') (result ' + resultType + ') (' + resultType + '.' + opcode + '/' + paramType + ' (get_local 0))) (export "" 0))')(op), expect);

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

//testConversion('i32', 'wrap', 'i64', 4294967336, 40); // TODO: NYI
testConversion('i32', 'trunc_s', 'f32', 40.1, 40);
testConversion('i32', 'trunc_u', 'f32', 40.1, 40);
testConversion('i32', 'trunc_s', 'f64', 40.1, 40);
testConversion('i32', 'trunc_u', 'f64', 40.1, 40);
//testConversion('i32', 'reinterpret', 'f32', 40.1, 1109419622); // TODO: NYI

//testConversion('i64', 'extend_s', 'i32', -2, -2); / TODO: NYI
//testConversion('i64', 'extend_u', 'i32', -2, 0xfffffffffffffffc); / TODO: NYI
//testConversion('i64', 'trunc_s', 'f32', 40.1, 40); // TODO: NYI
//testConversion('i64', 'trunc_u', 'f32', 40.1, 40); // TODO: NYI
//testConversion('i64', 'trunc_s', 'f64', 40.1, 40); // TODO: NYI
//testConversion('i64', 'trunc_u', 'f64', 40.1, 40); // TODO: NYI
//testConversion('i64', 'reinterpret', 'f64', 40.1, 1109419622); // TODO: NYI

testConversion('f32', 'convert_s', 'i32', 40, 40);
testConversion('f32', 'convert_u', 'i32', 40, 40);
//testConversion('f32', 'convert_s', 'i64', 40, 40); // TODO: NYI
//testConversion('f32', 'convert_u', 'i64', 40, 40); // TODO: NYI
testConversion('f32', 'demote', 'f64', 40.1, 40.099998474121094);
//testConversion('f32', 'reinterpret', 'i32', 40, 5.605193857299268e-44); // TODO: NYI

testConversion('f64', 'convert_s', 'i32', 40, 40);
testConversion('f64', 'convert_u', 'i32', 40, 40);
//testConversion('f64', 'convert_s', 'i64', 40, 40); // TODO: NYI
//testConversion('f64', 'convert_u', 'i64', 40, 40); // TODO: NYI
testConversion('f64', 'promote', 'f32', 40.1, 40.099998474121094);
//testConversion('f64', 'reinterpret', 'i64', 40.1, 4630840390592548045); // TODO: NYI
