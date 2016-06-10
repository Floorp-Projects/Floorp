// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

function testLoad(type, ext, base, offset, align, expect) {
  assertEq(wasmEvalText(
    '(module' +
    '  (memory 1' +
    '    (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")' +
    '    (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")' +
    '  )' +
    '  (func (param i32) (result ' + type + ')' +
    '    (' + type + '.load' + ext +
    '     offset=' + offset +
    '     ' + (align != 0 ? 'align=' + align : '') +
    '     (get_local 0)' +
    '    )' +
    '  ) (export "" 0))'
  )(base), expect);
}

function testStore(type, ext, base, offset, align, value) {
  assertEq(wasmEvalText(
    '(module' +
    '  (memory 1' +
    '    (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")' +
    '    (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")' +
    '  )' +
    '  (func (param i32) (param ' + type + ') (result ' + type + ')' +
    '    (' + type + '.store' + ext +
    '     offset=' + offset +
    '     ' + (align != 0 ? 'align=' + align : '') +
    '     (get_local 0)' +
    '     (get_local 1)' +
    '    )' +
    '  ) (export "" 0))'
  )(base, value), value);
}

function testLoadError(type, ext, base, offset, align, errorMsg) {
  assertErrorMessage(() => wasmEvalText(
    '(module' +
    '  (memory 1' +
    '    (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")' +
    '    (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")' +
    '  )' +
    '  (func (param i32) (result ' + type + ')' +
    '    (' + type + '.load' + ext +
    '     offset=' + offset +
    '     ' + (align != 0 ? 'align=' + align : '') +
    '     (get_local 0)' +
    '    )' +
    '  ) (export "" 0))'
  ), Error, errorMsg);
}

function testStoreError(type, ext, base, offset, align, errorMsg) {
  assertErrorMessage(() => wasmEvalText(
    '(module' +
    '  (memory 1' +
    '    (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")' +
    '    (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")' +
    '  )' +
    '  (func (param i32) (param ' + type + ') (result ' + type + ')' +
    '    (' + type + '.store' + ext +
    '     offset=' + offset +
    '     ' + (align != 0 ? 'align=' + align : '') +
    '     (get_local 0)' +
    '     (get_local 1)' +
    '    )' +
    '  ) (export "" 0))'
  ), Error, errorMsg);
}

testLoad('i32', '', 0, 0, 0, 0x03020100);
testLoad('i32', '', 1, 0, 0, 0x04030201);
testLoad('i32', '', 0, 4, 0, 0x07060504);
testLoad('i32', '', 1, 3, 4, 0x07060504);
//testLoad('i64', '', 0, 0, 0, 0x0001020304050607); // TODO: i64 NYI
//testLoad('i64', '', 1, 0, 0, 0x0102030405060708); // TODO: i64 NYI
//testLoad('i64', '', 0, 1, 0, 0x0102030405060708); // TODO: i64 NYI
//testLoad('i64', '', 1, 1, 4, 0x0203040506070809); // TODO: i64 NYI
testLoad('f32', '', 0, 0, 0, 3.820471434542632e-37);
testLoad('f32', '', 1, 0, 0, 1.539989614439558e-36);
testLoad('f32', '', 0, 4, 0, 1.0082513512365273e-34);
testLoad('f32', '', 1, 3, 4, 1.0082513512365273e-34);
testLoad('f64', '', 0, 0, 0, 7.949928895127363e-275);
testLoad('f64', '', 1, 0, 0, 5.447603722011605e-270);
testLoad('f64', '', 0, 8, 0, 3.6919162048650923e-236);
testLoad('f64', '', 1, 7, 4, 3.6919162048650923e-236);

testLoad('i32', '8_s', 16, 0, 0, -0x10);
testLoad('i32', '8_u', 16, 0, 0, 0xf0);
testLoad('i32', '16_s', 16, 0, 0, -0xe10);
testLoad('i32', '16_u', 16, 0, 0, 0xf1f0);

// When these tests fail, uncomment the load/store tests below.
function testLoadNYI(ext) {
    assertErrorMessage(() => wasmEvalText(`(module (memory 1) (func (i64.load${ext} (i32.const 0))))`), TypeError, /not yet implemented: i64/);
}
testLoadNYI('');
testLoadNYI('8_s');
testLoadNYI('8_u');
testLoadNYI('16_s');
testLoadNYI('16_u');
testLoadNYI('32_s');
testLoadNYI('32_u');
//testLoad('i64', '8_s', 16, 0, 0, -0x8); // TODO: i64 NYI
//testLoad('i64', '8_u', 16, 0, 0, 0x8); // TODO: i64 NYI
//testLoad('i64', '16_s', 16, 0, 0, -0x707); // TODO: i64 NYI
//testLoad('i64', '16_u', 16, 0, 0, 0x8f9); // TODO: i64 NYI
//testLoad('i64', '32_s', 16, 0, 0, -0x7060505); // TODO: i64 NYI
//testLoad('i64', '32_u', 16, 0, 0, 0x8f9fafb); // TODO: i64 NYI

testStore('i32', '', 0, 0, 0, -0x3f3e2c2c);
testStore('i32', '', 1, 0, 0, -0x3f3e2c2c);
testStore('i32', '', 0, 1, 0, -0x3f3e2c2c);
testStore('i32', '', 1, 1, 4, -0x3f3e2c2c);

function testStoreNYI(ext) {
    assertErrorMessage(() => wasmEvalText(`(module (memory 1) (func (i64.store${ext} (i32.const 0) (i32.const 0))))`), TypeError, /not yet implemented: i64/);
}
testStoreNYI('');
testStoreNYI('8');
testStoreNYI('16');
testStoreNYI('32');
//testStore('i64', '', 0, 0, 0, 0xc0c1d3d4e6e7090a); // TODO: i64 NYI
//testStore('i64', '', 1, 0, 0, 0xc0c1d3d4e6e7090a); // TODO: i64 NYI
//testStore('i64', '', 0, 1, 0, 0xc0c1d3d4e6e7090a); // TODO: i64 NYI
//testStore('i64', '', 1, 1, 4, 0xc0c1d3d4e6e7090a); // TODO: i64 NYI
//testStore('i64', '8', 0, 0, 0, 0x23); // TODO: i64 NYI
//testStore('i64', '16', 0, 0, 0, 0x23); // TODO: i64 NYI
//testStore('i64', '32', 0, 0, 0, 0x23); // TODO: i64 NYI

testStore('f32', '', 0, 0, 0, 0.01234566979110241);
testStore('f32', '', 1, 0, 0, 0.01234566979110241);
testStore('f32', '', 0, 4, 0, 0.01234566979110241);
testStore('f32', '', 1, 3, 4, 0.01234566979110241);
testStore('f64', '', 0, 0, 0, 0.89012345);
testStore('f64', '', 1, 0, 0, 0.89012345);
testStore('f64', '', 0, 8, 0, 0.89012345);
testStore('f64', '', 1, 7, 4, 0.89012345);

testStore('i32', '8', 0, 0, 0, 0x23);
testStore('i32', '16', 0, 0, 0, 0x2345);

testLoadError('i32', '8_s', 0, 0, 2, /greater than natural alignment/);
testLoadError('i32', '8_u', 0, 0, 2, /greater than natural alignment/);
testLoadError('i32', '16_s', 0, 0, 4, /greater than natural alignment/);
testLoadError('i32', '16_u', 0, 0, 4, /greater than natural alignment/);
testLoadError('i32', '', 0, 0, 8, /greater than natural alignment/);
testLoadError('f32', '', 0, 0, 8, /greater than natural alignment/);
testLoadError('f64', '', 0, 0, 16, /greater than natural alignment/);
testStoreError('i32', '8', 0, 0, 2, /greater than natural alignment/);
testStoreError('i32', '16', 0, 0, 4, /greater than natural alignment/);
testStoreError('i32', '', 0, 0, 8, /greater than natural alignment/);
testStoreError('f32', '', 0, 0, 8, /greater than natural alignment/);
testStoreError('f64', '', 0, 0, 16, /greater than natural alignment/);

assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (f64.store offset=0 (i32.const 0) (i32.const 0))))'), TypeError, mismatchError("i32", "f64"));
assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (f64.store offset=0 (i32.const 0) (f32.const 0))))'), TypeError, mismatchError("f32", "f64"));

assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (f32.store offset=0 (i32.const 0) (i32.const 0))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (f32.store offset=0 (i32.const 0) (f64.const 0))))'), TypeError, mismatchError("f64", "f32"));

assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (i32.store offset=0 (i32.const 0) (f32.const 0))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (i32.store offset=0 (i32.const 0) (f64.const 0))))'), TypeError, mismatchError("f64", "i32"));

wasmEvalText('(module (memory 0 65535))')
assertErrorMessage(() => wasmEvalText('(module (memory 0 65536))'), TypeError, /maximum memory size too big/);
