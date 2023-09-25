// Cloned from memory.js but kept separate because it may have to be disabled on
// some devices until bugs are fixed.

// Bug 1666747 - partially OOB unaligned stores are not handled correctly on ARM
// and ARM64.  The simulators don't implement the correct semantics anyhow, so
// when the bug is fixed in the code generator they must remain excluded here.
var excluded = getBuildConfiguration("arm64") ||
               getBuildConfiguration("arm64-simulator") ||
               getBuildConfiguration("arm") ||
               getBuildConfiguration("arm-simulator");
var thirtytwobit = getBuildConfiguration("pointer-byte-size") == 4;

const RuntimeError = WebAssembly.RuntimeError;

function storeModuleSrc(type, ext, offset, align) {
    var load_ext = ext === '' ? '' : ext + '_s';
    return `(module
       (memory (export "mem") 1)
       (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
       (data (i32.const 16) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (func $store (param i32) (param ${type})
         (${type}.store${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (local.get 0)
          (local.get 1)
         )
       ) (export "store" (func 0))
       (func $load (param i32) (result ${type})
        (${type}.load${load_ext}
         offset=${offset}
         ${align != 0 ? 'align=' + align : ''}
         (local.get 0)
        )
       ) (export "load" (func 1)))`;
}

function storeModule(type, ext, offset, align, exportBox = null) {
    let exports = wasmEvalText(storeModuleSrc(type, ext, offset, align)).exports;
    if (exportBox !== null)
        exportBox.exports = exports;
    return exports;
}

function testStoreOOB(type, ext, base, offset, align, value) {
    let exportBox = {};
    if (type === 'i64') {
        assertErrorMessage(() => wasmAssert(
            storeModuleSrc(type, ext, offset, align),
            [{type, func: '$store', args: [`i32.const ${base}`, `i64.const ${value}`]}],
            {},
            exportBox
        ), RuntimeError, /index out of bounds/);
    } else {
        assertErrorMessage(() => storeModule(type, ext, offset, align, exportBox).store(base, value),
                           RuntimeError,
                           /index out of bounds/);
    }

    // Check that there were no partial writes at the end of the memory.
    let buf = new Int8Array(exportBox.exports.mem.buffer);
    let len = buf.length;
    for ( let addr = base + offset ; addr < len; addr++ )
        assertEq(buf[addr], 0);
}

// Test bounds checks and edge cases.

for (let align of [0,1,2,4]) {

    for (let offset of [0, 1, 2, 3, 4, 8, 16, 41, 0xfff0, 0xfff8]) {
        // Accesses of 1 byte.
        let lastValidIndex = 0x10000 - 1 - offset;
        if (align < 2 && !excluded) {
            testStoreOOB('i32', '8', lastValidIndex + 1, offset, align, -42);
        }

        // Accesses of 2 bytes.
        lastValidIndex = 0x10000 - 2 - offset;
        if (align < 4 && !excluded) {
            testStoreOOB('i32', '16', lastValidIndex + 1, offset, align, -32768);
        }

        // Accesses of 4 bytes.
        lastValidIndex = 0x10000 - 4 - offset;
        if (!excluded) {
            testStoreOOB('i32', '', lastValidIndex + 1, offset, align, 1337);
            testStoreOOB('f32', '', lastValidIndex + 1, offset, align, Math.fround(13.37));
        }

        // Accesses of 8 bytes.
        lastValidIndex = 0x10000 - 8 - offset;
        if (!excluded) {
            testStoreOOB('f64', '', lastValidIndex + 1, offset, align, 1.23456789);
        }
    }

    for (let offset of [0, 1, 2, 3, 4, 8, 16, 41, 0xfff0, 0xfff8]) {
        // Accesses of 1 byte.
        let lastValidIndex = 0x10000 - 1 - offset;
        if (align < 2 && !excluded) {
            testStoreOOB('i64', '8', lastValidIndex + 1, offset, align, -42);
        }

        // Accesses of 2 bytes.
        lastValidIndex = 0x10000 - 2 - offset;
        if (align < 4 && !excluded) {
            testStoreOOB('i64', '16', lastValidIndex + 1, offset, align, -32768);
        }

        // Accesses of 4 bytes.
        lastValidIndex = 0x10000 - 4 - offset;
        if (!excluded) {
            testStoreOOB('i64', '32', lastValidIndex + 1, offset, align, 0xf1231337 | 0);
        }

        // Accesses of 8 bytes.
        lastValidIndex = 0x10000 - 8 - offset;
        if (!excluded) {
            testStoreOOB('i64', '', lastValidIndex + 1, offset, align, '0x1234567887654321');
        }
    }
}

// On 32-bit platforms, a declared-aligned i64 store is implemented as two
// presumed-aligned 32-bit stores.  This tests that we don't store the low
// word before the high word if the low word is in-bounds but the high word
// is not.
if (thirtytwobit) {
    testStoreOOB('i64', '', 0x10000 - 4, 0, 0, '0x0123456789abcdef');
}
