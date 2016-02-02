load(libdir + "wasm.js");

if (!wasmIsSupported())
    quit();

function mismatchError(actual, expect) {
    var str = "type mismatch: expression has type " + actual + " but expected " + expect;
    return RegExp(str);
}

function testConversion(resultType, opcode, paramType, op, expect) {
  assertEq(wasmEvalText('(module (func (param ' + paramType + ') (result ' + resultType + ') (' + resultType + '.' + opcode + '/' + paramType + ' (get_local 0))) (export "" 0))')(op), expect);

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

testConversion('i32', 'trunc_s', 'f32', 40.1, 40);
testConversion('i32', 'trunc_u', 'f32', 40.1, 40);
testConversion('i32', 'trunc_s', 'f64', 40.1, 40);
testConversion('i32', 'trunc_u', 'f64', 40.1, 40);
//testConversion('i32', 'reinterpret', 'f32', 40.1, 1109419622); // TODO: NYI

testConversion('f32', 'convert_s', 'i32', 40, 40);
testConversion('f32', 'convert_u', 'i32', 40, 40);
testConversion('f32', 'demote', 'f64', 40.1, 40.099998474121094);
//testConversion('f32', 'reinterpret', 'i32', 40, 5.605193857299268e-44); // TODO: NYI

testConversion('f64', 'convert_s', 'i32', 40, 40);
testConversion('f64', 'convert_u', 'i32', 40, 40);
testConversion('f64', 'promote', 'f32', 40.1, 40.099998474121094);
