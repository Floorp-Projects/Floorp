load(libdir + "wasm.js");

quit(); // TODO: Loads and stores are NYI.

if (!wasmIsSupported())
    quit();

function mismatchError(actual, expect) {
    var str = "type mismatch: expression has type " + actual + " but expected " + expect;
    return RegExp(str);
}

function testLoad(type, ext, base, offset, align, expect) {
    print('(module' +
    '  (memory 0x10000' +
    '    (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")' +
    '    (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")' +
    '  )' +
    '  (func (param i32) (result ' + type + ')' +
    '    (' + type + '.load' + ext + ' (get_local 0)' +
    '     offset=' + offset +
    '     ' + (align != 0 ? 'align=' + align : '') +
    '    )' +
    '  ) (export "" 0))');
  assertEq(wasmEvalText(
    '(module' +
    '  (memory 0x10000' +
    '    (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")' +
    '    (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")' +
    '  )' +
    '  (func (param i32) (result ' + type + ')' +
    '    (' + type + '.load' + ext + ' (get_local 0)' +
    '     offset=' + offset +
    '     ' + (align != 0 ? 'align=' + align : '') +
    '    )' +
    '  ) (export "" 0))'
  )(base), expect);
}

function testStore(type, ext, base, offset, align, value) {
  assertEq(wasmEvalText(
    '(module' +
    '  (memory 0x10000)' +
    '    (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")' +
    '    (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")' +
    '  )' +
    '  (func (param i32) (param ' + type + ') (result ' + type + ')' +
    '    (' + type + '.store' + ext + ' (get_local 0)' +
    '     offset=' + offset +
    '     ' + (align != 0 ? 'align=' + align : '') +
    '     (get_local 1)' +
    '    )' +
    '  ) (export "" 0))'
  )(base, value), value);
}

function testConstError(type, str) {
  // For now at least, we don't distinguish between parse errors and OOMs.
  assertErrorMessage(() => wasmEvalText('(module (func (result ' + type + ') (' + type + '.const ' + str + ')) (export "" 0))')(), Error, /out of memory/);
}

testLoad('i32', '', 0, 0, 0, 0x00010203);
testLoad('i32', '', 1, 0, 0, 0x01020304);
//testLoad('i32', '', 0, 1, 0, 0x01020304); // TODO: NYI
//testLoad('i32', '', 1, 1, 4, 0x02030405); // TODO: NYI
//testLoad('i64', '', 0, 0, 0, 0x0001020304050607); // TODO: NYI
//testLoad('i64', '', 1, 0, 0, 0x0102030405060708); // TODO: NYI
//testLoad('i64', '', 0, 1, 0, 0x0102030405060708); // TODO: NYI
//testLoad('i64', '', 1, 1, 4, 0x0203040506070809); // TODO: NYI
testLoad('f32', '', 0, 0, 0, 0x00010203);
testLoad('f32', '', 1, 0, 0, 0x01020304);
//testLoad('f32', '', 0, 1, 0, 0x01020304); // TODO: NYI
//testLoad('f32', '', 1, 1, 4, 0x02030405); // TODO: NYI
testLoad('f64', '', 0, 0, 0, 0x00010203);
testLoad('f64', '', 1, 0, 0, 0x01020304);
//testLoad('f64', '', 0, 1, 0, 0x01020304); // TODO: NYI
//testLoad('f64', '', 1, 1, 4, 0x02030405); // TODO: NYI

testLoad('i32', '8_s', 16, 0, 0, -0x8);
testLoad('i32', '8_u', 16, 0, 0, 0x8);
testLoad('i32', '16_s', 16, 0, 0, -0x707);
testLoad('i32', '16_u', 16, 0, 0, 0x8f9);
testLoad('i64', '8_s', 16, 0, 0, -0x8);
testLoad('i64', '8_u', 16, 0, 0, 0x8);
//testLoad('i64', '16_s', 16, 0, 0, -0x707); // TODO: NYI
//testLoad('i64', '16_u', 16, 0, 0, 0x8f9); // TODO: NYI
//testLoad('i64', '32_s', 16, 0, 0, -0x7060505); // TODO: NYI
//testLoad('i64', '32_u', 16, 0, 0, 0x8f9fafb); // TODO: NYI

testStore('i32', '', 0, 0, 0, 0xc0c1d3d4);
testStore('i32', '', 1, 0, 0, 0xc0c1d3d4);
//testStore('i32', '', 0, 1, 0, 0xc0c1d3d4); // TODO: NYI
//testStore('i32', '', 1, 1, 4, 0xc0c1d3d4); // TODO: NYI
//testStore('i64', '', 0, 0, 0, 0xc0c1d3d4e6e7090a); // TODO: NYI
//testStore('i64', '', 1, 0, 0, 0xc0c1d3d4e6e7090a); // TODO: NYI
//testStore('i64', '', 0, 1, 0, 0xc0c1d3d4e6e7090a); // TODO: NYI
//testStore('i64', '', 1, 1, 4, 0xc0c1d3d4e6e7090a); // TODO: NYI
testStore('f32', '', 0, 0, 0, 0.01234567);
testStore('f32', '', 1, 0, 0, 0.01234567);
//testStore('f32', '', 0, 1, 0, 0.01234567); // TODO: NYI
//testStore('f32', '', 1, 1, 4, 0.01234567); // TODO: NYI
testStore('f64', '', 0, 0, 0, 0.89012345);
testStore('f64', '', 1, 0, 0, 0.89012345);
//testStore('f64', '', 0, 1, 0, 0.89012345); // TODO: NYI
//testStore('f64', '', 1, 1, 4, 0.89012345); // TODO: NYI

testStore('i32', '8', 0, 0, 0, 0x23);
testStore('i32', '16', 0, 0, 0, 0x2345);
//testStore('i64', '8', 0, 0, 0, 0x23); // TODO: NYI
//testStore('i64', '16', 0, 0, 0, 0x23); // TODO: NYI
//testStore('i64', '32', 0, 0, 0, 0x23); // TODO: NYI
