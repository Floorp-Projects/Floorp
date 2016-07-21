// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

function loadModule(type, ext, offset, align) {
    return wasmEvalText(
    `(module
       (memory 1
         (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
         (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       )
       (func (param i32) (result ${type})
         (${type}.load${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (get_local 0)
         )
       ) (export "" 0))`
    );
}

function storeModule(type, ext, offset, align) {
    var load_ext = ext === '' ? '' : ext + '_s';
    return wasmEvalText(
    `(module
       (memory 1
         (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
         (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       )
       (func (param i32) (param ${type}) (result ${type})
         (${type}.store${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (get_local 0)
          (get_local 1)
         )
       ) (export "store" 0)
       (func (param i32) (result ${type})
        (${type}.load${load_ext}
         offset=${offset}
         ${align != 0 ? 'align=' + align : ''}
         (get_local 0)
        )
       ) (export "load" 1))`
    );
}

function storeModuleCst(type, ext, offset, align, value) {
    var load_ext = ext === '' ? '' : ext + '_s';
    return wasmEvalText(
    `(module
       (memory 1
         (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
         (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       )
       (func (param i32) (result ${type})
         (${type}.store${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (get_local 0)
          (${type}.const ${value})
         )
       ) (export "store" 0)
       (func (param i32) (result ${type})
        (${type}.load${load_ext}
         offset=${offset}
         ${align != 0 ? 'align=' + align : ''}
         (get_local 0)
        )
       ) (export "load" 1))`
    );
}

function testLoad(type, ext, base, offset, align, expect) {
    if (type === 'i64')
        assertEqI64(loadModule(type, ext, offset, align)(base), createI64(expect));
    else
        assertEq(loadModule(type, ext, offset, align)(base), expect);
}

function testLoadOOB(type, ext, base, offset, align) {
    assertErrorMessage(() => loadModule(type, ext, offset, align)(base), Error, /invalid or out-of-range index/);
}

function testStore(type, ext, base, offset, align, value) {
    let module = storeModule(type, ext, offset, align);
    let moduleCst = storeModuleCst(type, ext, offset, align, value);
    if (type === 'i64') {
        var i64 = createI64(value);
        assertEqI64(module.store(base, i64), i64);
        assertEqI64(module.load(base), i64);
        assertEqI64(moduleCst.store(base), i64);
        assertEqI64(moduleCst.load(base), i64);
    } else {
        assertEq(module.store(base, value), value);
        assertEq(module.load(base), value);
        assertEq(moduleCst.store(base), value);
        assertEq(moduleCst.load(base), value);
    }
}

function testStoreOOB(type, ext, base, offset, align, value) {
    if (type === 'i64')
        value = createI64(value);
    assertErrorMessage(() => storeModule(type, ext, offset, align).store(base, value), Error, /invalid or out-of-range index/);
}

// Test bounds checks and edge cases.
const align = 0;

for (var ind = 0; ind < 2; ind++) {
  if (ind == 1)
    setJitCompilerOption('wasm.explicit-bounds-checks', 1);

  testLoad('i32', '', 0, 0, 0, 0x03020100);
  testLoad('i32', '', 1, 0, 0, 0x04030201);
  testLoad('i32', '', 0, 4, 0, 0x07060504);
  testLoad('i32', '', 1, 3, 4, 0x07060504);
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

  testStore('i32', '', 0, 0, 0, -0x3f3e2c2c);
  testStore('i32', '', 1, 0, 0, -0x3f3e2c2c);
  testStore('i32', '', 0, 1, 0, -0x3f3e2c2c);
  testStore('i32', '', 1, 1, 4, -0x3f3e2c2c);

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

  assertErrorMessage(() => wasmEvalText('(module (memory 2 1))'), TypeError, /maximum memory size less than initial memory size/);

  for (let offset of [0, 1, 2, 3, 4, 8, 16, 41, 0xfff8]) {
      // Accesses of 1 byte.
      let lastValidIndex = 0x10000 - 1 - offset;

      testLoad('i32', '8_s', lastValidIndex, offset, align, 0);
      testLoadOOB('i32', '8_s', lastValidIndex + 1, offset, align);

      testLoad('i32', '8_u', lastValidIndex, offset, align, 0);
      testLoadOOB('i32', '8_u', lastValidIndex + 1, offset, align);

      testStore('i32', '8', lastValidIndex, offset, align, -42);
      testStoreOOB('i32', '8', lastValidIndex + 1, offset, align, -42);

      // Accesses of 2 bytes.
      lastValidIndex = 0x10000 - 2 - offset;

      testLoad('i32', '16_s', lastValidIndex, offset, align, 0);
      testLoadOOB('i32', '16_s', lastValidIndex + 1, offset, align);

      testLoad('i32', '16_u', lastValidIndex, offset, align, 0);
      testLoadOOB('i32', '16_u', lastValidIndex + 1, offset, align);

      testStore('i32', '16', lastValidIndex, offset, align, -32768);
      testStoreOOB('i32', '16', lastValidIndex + 1, offset, align, -32768);

      // Accesses of 4 bytes.
      lastValidIndex = 0x10000 - 4 - offset;

      testLoad('i32', '', lastValidIndex, offset, align, 0);
      testLoadOOB('i32', '', lastValidIndex + 1, offset, align);

      testLoad('f32', '', lastValidIndex, offset, align, 0);
      testLoadOOB('f32', '', lastValidIndex + 1, offset, align);

      testStore('i32', '', lastValidIndex, offset, align, 1337);
      testStoreOOB('i32', '', lastValidIndex + 1, offset, align, 1337);

      testStore('f32', '', lastValidIndex, offset, align, Math.fround(13.37));
      testStoreOOB('f32', '', lastValidIndex + 1, offset, align, Math.fround(13.37));

      // Accesses of 8 bytes.
      lastValidIndex = 0x10000 - 8 - offset;

      testLoad('f64', '', lastValidIndex, offset, align, 0);
      testLoadOOB('f64', '', lastValidIndex + 1, offset, align);

      testStore('f64', '', lastValidIndex, offset, align, 1.23456789);
      testStoreOOB('f64', '', lastValidIndex + 1, offset, align, 1.23456789);
  }

  // Ensure wrapping doesn't apply.
  offset = 0x7fffffff; // maximum allowed offset that doesn't always throw.
  for (let index of [0, 1, 2, 3, 0x7fffffff, 0x80000000, 0x80000001]) {
      testLoadOOB('i32', '8_s', index, offset, align);
      testLoadOOB('i32', '16_s', index, offset, align);
      testLoadOOB('i32', '', index, offset, align);
      testLoadOOB('f32', '', index, offset, align);
      testLoadOOB('f64', '', index, offset, align);
  }

  assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (f64.store offset=0 (i32.const 0) (i32.const 0))))'), TypeError, mismatchError("i32", "f64"));
  assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (f64.store offset=0 (i32.const 0) (f32.const 0))))'), TypeError, mismatchError("f32", "f64"));

  assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (f32.store offset=0 (i32.const 0) (i32.const 0))))'), TypeError, mismatchError("i32", "f32"));
  assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (f32.store offset=0 (i32.const 0) (f64.const 0))))'), TypeError, mismatchError("f64", "f32"));

  assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (i32.store offset=0 (i32.const 0) (f32.const 0))))'), TypeError, mismatchError("f32", "i32"));
  assertErrorMessage(() => wasmEvalText('(module (memory 1) (func (i32.store offset=0 (i32.const 0) (f64.const 0))))'), TypeError, mismatchError("f64", "i32"));

  wasmEvalText('(module (memory 0 65535))')
  assertErrorMessage(() => wasmEvalText('(module (memory 0 65536))'), TypeError, /maximum memory size too big/);

  // Test high charge of registers
  function testRegisters() {
      assertEq(wasmEvalText(
      `(module
         (memory 1
           (segment 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
           (segment 16 "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
         )
         (func (param i32) (local i32 i32 i32 i32 f32 f64) (result i32)
           (set_local 1 (i32.load8_s offset=4 (get_local 0)))
           (set_local 2 (i32.load16_s (get_local 1)))
           (i32.store8 offset=4 (get_local 0) (get_local 1))
           (set_local 3 (i32.load16_u (get_local 2)))
           (i32.store16 (get_local 1) (get_local 2))
           (set_local 4 (i32.load (get_local 2)))
           (i32.store (get_local 1) (get_local 2))
           (set_local 5 (f32.load (get_local 4)))
           (f32.store (get_local 4) (get_local 5))
           (set_local 6 (f64.load (get_local 4)))
           (f64.store (get_local 4) (get_local 6))
           (i32.add
            (i32.add
             (get_local 0)
             (get_local 1)
            )
            (i32.add
             (i32.add
              (get_local 2)
              (get_local 3)
             )
             (i32.add
              (get_local 4)
              (i32.reinterpret/f32 (get_local 5))
             )
            )
           )
         ) (export "" 0))`
      )(1), 50464523);
  }

  testRegisters();

  if (hasI64()) {
      setJitCompilerOption('wasm.test-mode', 1);

      testLoad('i64', '', 0, 0, 0, '0x0706050403020100');
      testLoad('i64', '', 1, 0, 0, '0x0807060504030201');
      testLoad('i64', '', 0, 1, 0, '0x0807060504030201');
      testLoad('i64', '', 1, 1, 4, '0x0908070605040302');

      testLoad('i64', '8_s', 16, 0, 0, -0x10);
      testLoad('i64', '8_u', 16, 0, 0, 0xf0);
      testLoad('i64', '16_s', 16, 0, 0, -0xe10);
      testLoad('i64', '16_u', 16, 0, 0, 0xf1f0);
      testLoad('i64', '32_s', 16, 0, 0, 0xf3f2f1f0 | 0);
      testLoad('i64', '32_u', 16, 0, 0, '0xf3f2f1f0');

      testStore('i64', '', 0, 0, 0, '0xc0c1d3d4e6e7090a');
      testStore('i64', '', 1, 0, 0, '0xc0c1d3d4e6e7090a');
      testStore('i64', '', 0, 1, 0, '0xc0c1d3d4e6e7090a');
      testStore('i64', '', 1, 1, 4, '0xc0c1d3d4e6e7090a');
      testStore('i64', '8', 0, 0, 0, 0x23);
      testStore('i64', '16', 0, 0, 0, 0x23);
      testStore('i64', '32', 0, 0, 0, 0x23);

      let align = 0;
      for (let offset of [0, 1, 2, 3, 4, 8, 16, 41, 0xfff8]) {
          // Accesses of 1 byte.
          let lastValidIndex = 0x10000 - 1 - offset;

          testLoad('i64', '8_s', lastValidIndex, offset, align, 0);
          testLoadOOB('i64', '8_s', lastValidIndex + 1, offset, align);

          testLoad('i64', '8_u', lastValidIndex, offset, align, 0);
          testLoadOOB('i64', '8_u', lastValidIndex + 1, offset, align);

          testStore('i64', '8', lastValidIndex, offset, align, -42);
          testStoreOOB('i64', '8', lastValidIndex + 1, offset, align, -42);

          // Accesses of 2 bytes.
          lastValidIndex = 0x10000 - 2 - offset;

          testLoad('i64', '16_s', lastValidIndex, offset, align, 0);
          testLoadOOB('i64', '16_s', lastValidIndex + 1, offset, align);

          testLoad('i64', '16_u', lastValidIndex, offset, align, 0);
          testLoadOOB('i64', '16_u', lastValidIndex + 1, offset, align);

          testStore('i64', '16', lastValidIndex, offset, align, -32768);
          testStoreOOB('i64', '16', lastValidIndex + 1, offset, align, -32768);

          // Accesses of 4 bytes.
          lastValidIndex = 0x10000 - 4 - offset;

          testLoad('i64', '32_s', lastValidIndex, offset, align, 0);
          testLoadOOB('i64', '32_s', lastValidIndex + 1, offset, align);

          testLoad('i64', '32_u', lastValidIndex, offset, align, 0);
          testLoadOOB('i64', '32_u', lastValidIndex + 1, offset, align);

          testStore('i64', '32', lastValidIndex, offset, align, 0xf1231337 | 0);
          testStoreOOB('i64', '32', lastValidIndex + 1, offset, align, 0xf1231337 | 0);

          // Accesses of 8 bytes.
          lastValidIndex = 0x10000 - 8 - offset;

          testLoad('i64', '', lastValidIndex, offset, align, 0);
          testLoadOOB('i64', '', lastValidIndex + 1, offset, align);

          testStore('i64', '', lastValidIndex, offset, align, '0x1234567887654321');
          testStoreOOB('i64', '', lastValidIndex + 1, offset, align, '0x1234567887654321');
      }

      setJitCompilerOption('wasm.test-mode', 0);
  }
}
