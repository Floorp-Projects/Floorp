const RuntimeError = WebAssembly.RuntimeError;

function loadModuleSrc(type, ext, offset, align, drop = false) {
    let maybeResult = drop ? '' : `(result ${type})`;
    let maybeDrop = drop ? 'drop' : '';
    return `(module
       (memory 1)
       (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
       (data (i32.const 16) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (func $load (param i32) ${maybeResult}
         (${type}.load${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (local.get 0)
         )
         ${maybeDrop}
       ) (export "" 0))`;
}
function loadModule(type, ext, offset, align, drop = false) {
    return wasmEvalText(loadModuleSrc(type, ext, offset, align, drop)).exports[""];
}

function storeModuleSrc(type, ext, offset, align) {
    var load_ext = ext === '' ? '' : ext + '_s';
    return `(module
       (memory 1)
       (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
       (data (i32.const 16) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (func $store (param i32) (param ${type})
         (${type}.store${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (local.get 0)
          (local.get 1)
         )
       ) (export "store" 0)
       (func $load (param i32) (result ${type})
        (${type}.load${load_ext}
         offset=${offset}
         ${align != 0 ? 'align=' + align : ''}
         (local.get 0)
        )
       ) (export "load" 1))`;
}
function storeModule(type, ext, offset, align) {
    return wasmEvalText(storeModuleSrc(type, ext, offset, align)).exports;
}

function storeModuleCstSrc(type, ext, offset, align, value) {
    var load_ext = ext === '' ? '' : ext + '_s';
    return `(module
       (memory 1)
       (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
       (data (i32.const 16) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (func $store (param i32)
         (${type}.store${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (local.get 0)
          (${type}.const ${value})
         )
       ) (export "store" 0)
       (func $load (param i32) (result ${type})
        (${type}.load${load_ext}
         offset=${offset}
         ${align != 0 ? 'align=' + align : ''}
         (local.get 0)
        )
       ) (export "load" 1))`;
}
function storeModuleCst(type, ext, offset, align, value) {
    return wasmEvalText(storeModuleCstSrc(type, ext, offset, align, value)).exports;
}

function testLoad(type, ext, base, offset, align, expect) {
    if (type === 'i64') {
        wasmAssert(loadModuleSrc(type, ext, offset, align), [{
            type,
            func: '$load',
            expected: expect,
            args: [`i32.const ${base}`]
        }]);
    }
    else {
        assertEq(loadModule(type, ext, offset, align)(base), expect);
    }
}

function testLoadOOB(type, ext, base, offset, align) {
    assertErrorMessage(() => loadModule(type, ext, offset, align, /*drop*/ true)(base), RuntimeError, /index out of bounds/);
}

function testStore(type, ext, base, offset, align, value) {
    if (type === 'i64') {
        wasmAssert(storeModuleSrc(type, ext, offset, align), [
            {type, func: '$store', args: [`i32.const ${base}`, `i64.const ${value}`]},
            {type, func: '$load', args: [`i32.const ${base}`], expected: value},
        ]);
        wasmAssert(storeModuleCstSrc(type, ext, offset, align, value), [
            {type, func: '$store', args: [`i32.const ${base}`]},
            {type, func: '$load', args: [`i32.const ${base}`], expected: value},
        ]);
    } else {
        let module = storeModule(type, ext, offset, align);
        let moduleCst = storeModuleCst(type, ext, offset, align, value);
        module.store(base, value);
        assertEq(module.load(base), value);
        moduleCst.store(base);
        assertEq(moduleCst.load(base), value);
    }
}

function testStoreOOB(type, ext, base, offset, align, value) {
    if (type === 'i64') {
        assertErrorMessage(() => wasmAssert(
            storeModuleSrc(type, ext, offset, align, value),
            [{type, func: '$store', args: [`i32.const ${base}`, `i64.const ${value}`]}]
        ), RuntimeError, /index out of bounds/);
    } else {
        assertErrorMessage(() => storeModule(type, ext, offset, align).store(base, value), RuntimeError, /index out of bounds/);
    }
}

function badLoadModule(type, ext) {
    wasmFailValidateText( `(module (func (param i32) (${type}.load${ext} (local.get 0))) (export "" 0))`, /can't touch memory/);
}

function badStoreModule(type, ext) {
    wasmFailValidateText(`(module (func (param i32) (${type}.store${ext} (local.get 0) (${type}.const 0))) (export "" 0))`, /can't touch memory/);
}

// Can't touch memory.
for (let [type, ext] of [
    ['i32', ''],
    ['i32', '8_s'],
    ['i32', '8_u'],
    ['i32', '16_s'],
    ['i32', '16_u'],
    ['i64', ''],
    ['i64', '8_s'],
    ['i64', '8_u'],
    ['i64', '16_s'],
    ['i64', '16_u'],
    ['i64', '32_s'],
    ['i64', '32_u'],
    ['f32', ''],
    ['f64', ''],
])
{
    badLoadModule(type, ext);
}

for (let [type, ext] of [
    ['i32', ''],
    ['i32', '8'],
    ['i32', '16'],
    ['i64', ''],
    ['i64', '8'],
    ['i64', '16'],
    ['i64', '32'],
    ['f32', ''],
    ['f64', ''],
])
{
    badStoreModule(type, ext);
}

assertEq(getJitCompilerOptions()['wasm.fold-offsets'], 1);

for (var foldOffsets = 0; foldOffsets <= 1; foldOffsets++) {
    setJitCompilerOption('wasm.fold-offsets', foldOffsets | 0);

    testLoad('i32', '', 0, 0, 0, 0x03020100);
    testLoad('i32', '', 1, 0, 1, 0x04030201);
    testLoad('i32', '', 0, 4, 0, 0x07060504);
    testLoad('i32', '', 1, 3, 4, 0x07060504);
    testLoad('f32', '', 0, 0, 0, 3.820471434542632e-37);
    testLoad('f32', '', 1, 0, 1, 1.539989614439558e-36);
    testLoad('f32', '', 0, 4, 0, 1.0082513512365273e-34);
    testLoad('f32', '', 1, 3, 4, 1.0082513512365273e-34);
    testLoad('f64', '', 0, 0, 0, 7.949928895127363e-275);
    testLoad('f64', '', 1, 0, 1, 5.447603722011605e-270);
    testLoad('f64', '', 0, 8, 0, 3.6919162048650923e-236);
    testLoad('f64', '', 1, 7, 8, 3.6919162048650923e-236);

    testLoad('i32', '8_s', 16, 0, 0, -0x10);
    testLoad('i32', '8_u', 16, 0, 0, 0xf0);
    testLoad('i32', '16_s', 16, 0, 0, -0xe10);
    testLoad('i32', '16_u', 16, 0, 0, 0xf1f0);

    testStore('i32', '', 0, 0, 0, -0x3f3e2c2c);
    testStore('i32', '', 1, 0, 1, -0x3f3e2c2c);
    testStore('i32', '', 0, 1, 1, -0x3f3e2c2c);
    testStore('i32', '', 1, 1, 4, -0x3f3e2c2c);

    testStore('f32', '', 0, 0, 0, 0.01234566979110241);
    testStore('f32', '', 1, 0, 1, 0.01234566979110241);
    testStore('f32', '', 0, 4, 0, 0.01234566979110241);
    testStore('f32', '', 1, 3, 4, 0.01234566979110241);
    testStore('f64', '', 0, 0, 0, 0.89012345);
    testStore('f64', '', 1, 0, 1, 0.89012345);
    testStore('f64', '', 0, 8, 0, 0.89012345);
    testStore('f64', '', 1, 7, 8, 0.89012345);

    testStore('i32', '8', 0, 0, 0, 0x23);
    testStore('i32', '16', 0, 0, 0, 0x2345);

    wasmFailValidateText('(module (memory 2 1))', /maximum length 1 is less than initial length 2/);

    // Test bounds checks and edge cases.

    for (let align of [0,1,2,4]) {

        for (let offset of [0, 1, 2, 3, 4, 8, 16, 41, 0xfff8]) {
            // Accesses of 1 byte.
            let lastValidIndex = 0x10000 - 1 - offset;

            if (align < 2) {
                testLoad('i32', '8_s', lastValidIndex, offset, align, 0);
                testLoadOOB('i32', '8_s', lastValidIndex + 1, offset, align);

                testLoad('i32', '8_u', lastValidIndex, offset, align, 0);
                testLoadOOB('i32', '8_u', lastValidIndex + 1, offset, align);

                testStore('i32', '8', lastValidIndex, offset, align, -42);
                testStoreOOB('i32', '8', lastValidIndex + 1, offset, align, -42);
            }

            // Accesses of 2 bytes.
            lastValidIndex = 0x10000 - 2 - offset;

            if (align < 4) {
                testLoad('i32', '16_s', lastValidIndex, offset, align, 0);
                testLoadOOB('i32', '16_s', lastValidIndex + 1, offset, align);

                testLoad('i32', '16_u', lastValidIndex, offset, align, 0);
                testLoadOOB('i32', '16_u', lastValidIndex + 1, offset, align);

                testStore('i32', '16', lastValidIndex, offset, align, -32768);
                testStoreOOB('i32', '16', lastValidIndex + 1, offset, align, -32768);
            }

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
            if (align < 2) {
                testLoadOOB('i32', '8_s', index, offset, align);
            }
            if (align < 4) {
                testLoadOOB('i32', '16_s', index, offset, align);
            }
            testLoadOOB('i32', '', index, offset, align);
            testLoadOOB('f32', '', index, offset, align);
            testLoadOOB('f64', '', index, offset, align);
        }

        // Ensure out of bounds when the offset is greater than the immediate range.
        index = 0;
        for (let offset of [0x80000000, 0xfffffffe, 0xffffffff]) {
            testLoadOOB('i32', '8_s', index, offset, 1);
            testLoadOOB('i32', '16_s', index, offset, 1);
            testLoadOOB('i32', '16_s', index, offset, 2);
            testLoadOOB('i32', '', index, offset, 1);
            testLoadOOB('i32', '', index, offset, 4);
            testLoadOOB('f32', '', index, offset, 1);
            testLoadOOB('f32', '', index, offset, 4);
            testLoadOOB('f64', '', index, offset, 1);
            testLoadOOB('f64', '', index, offset, 8);
        }

        wasmFailValidateText('(module (memory 1) (func (f64.store offset=0 (i32.const 0) (i32.const 0))))', mismatchError("i32", "f64"));
        wasmFailValidateText('(module (memory 1) (func (f64.store offset=0 (i32.const 0) (f32.const 0))))', mismatchError("f32", "f64"));

        wasmFailValidateText('(module (memory 1) (func (f32.store offset=0 (i32.const 0) (i32.const 0))))', mismatchError("i32", "f32"));
        wasmFailValidateText('(module (memory 1) (func (f32.store offset=0 (i32.const 0) (f64.const 0))))', mismatchError("f64", "f32"));

        wasmFailValidateText('(module (memory 1) (func (i32.store offset=0 (i32.const 0) (f32.const 0))))', mismatchError("f32", "i32"));
        wasmFailValidateText('(module (memory 1) (func (i32.store offset=0 (i32.const 0) (f64.const 0))))', mismatchError("f64", "i32"));

        // Test high number of registers.
        function testRegisters() {
            assertEq(wasmEvalText(
                `(module
              (memory 1)
              (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
              (data (i32.const 16) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
              (func (param i32) (local i32 i32 i32 i32 f32 f64) (result i32)
               (local.set 1 (i32.load8_s offset=4 (local.get 0)))
               (local.set 2 (i32.load16_s (local.get 1)))
               (i32.store8 offset=4 (local.get 0) (local.get 1))
               (local.set 3 (i32.load16_u (local.get 2)))
               (i32.store16 (local.get 1) (local.get 2))
               (local.set 4 (i32.load (local.get 2)))
               (i32.store (local.get 1) (local.get 2))
               (local.set 5 (f32.load (local.get 4)))
               (f32.store (local.get 4) (local.get 5))
               (local.set 6 (f64.load (local.get 4)))
               (f64.store (local.get 4) (local.get 6))
               (i32.add
                (i32.add
                 (local.get 0)
                 (local.get 1)
                )
                (i32.add
                 (i32.add
                  (local.get 2)
                  (local.get 3)
                 )
                 (i32.add
                  (local.get 4)
                  (i32.reinterpret/f32 (local.get 5))
                 )
                )
               )
              ) (export "" 0))`
            ).exports[""](1), 50464523);
        }

        testRegisters();

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

        for (let offset of [0, 1, 2, 3, 4, 8, 16, 41, 0xfff8]) {
            // Accesses of 1 byte.
            let lastValidIndex = 0x10000 - 1 - offset;

            if (align < 2) {
                testLoad('i64', '8_s', lastValidIndex, offset, align, 0);
                testLoadOOB('i64', '8_s', lastValidIndex + 1, offset, align);

                testLoad('i64', '8_u', lastValidIndex, offset, align, 0);
                testLoadOOB('i64', '8_u', lastValidIndex + 1, offset, align);

                testStore('i64', '8', lastValidIndex, offset, align, -42);
                testStoreOOB('i64', '8', lastValidIndex + 1, offset, align, -42);
            }

            // Accesses of 2 bytes.
            lastValidIndex = 0x10000 - 2 - offset;

            if (align < 4) {
                testLoad('i64', '16_s', lastValidIndex, offset, align, 0);
                testLoadOOB('i64', '16_s', lastValidIndex + 1, offset, align);

                testLoad('i64', '16_u', lastValidIndex, offset, align, 0);
                testLoadOOB('i64', '16_u', lastValidIndex + 1, offset, align);

                testStore('i64', '16', lastValidIndex, offset, align, -32768);
                testStoreOOB('i64', '16', lastValidIndex + 1, offset, align, -32768);
            }

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
    }
}

setJitCompilerOption('wasm.fold-offsets', 1);

// Test active segments with a memory index.

{
    function makeIt(flag, memindex) {
        return new Uint8Array([0x00, 0x61, 0x73, 0x6d,
                               0x01, 0x00, 0x00, 0x00,
                               0x05,                   // Memory section
                               0x03,                   // Section size
                               0x01,                   // One memory
                               0x00,                   // Unshared, min only
                               0x01,                   // Min
                               0x0b,                   // Data section
                               0x0a,                   // Section size
                               0x01,                   // One data segment
                               flag,                   // Flag should be 2, or > 2 if invalid
                               memindex,               // Memory index should be 0, or > 0 if invalid
                               0x41,                   // Init expr: i32.const
                               0x00,                   // Init expr: zero (payload)
                               0x0b,                   // Init expr: end
                               0x03,                   // Three bytes, because why not?
                               0x01,
                               0x02,
                               0x03]);
    }

    // Should succeed because this is what an active segment with index looks like
    new WebAssembly.Module(makeIt(0x02, 0x00));

    // Should fail because the kind is unknown
    assertErrorMessage(() => new WebAssembly.Module(makeIt(0x03, 0x00)),
                       WebAssembly.CompileError,
                       /invalid data initializer-kind/);

    // Should fail because the memory index is bad
    assertErrorMessage(() => new WebAssembly.Module(makeIt(0x02, 0x01)),
                       WebAssembly.CompileError,
                       /memory index must be zero/);
}
