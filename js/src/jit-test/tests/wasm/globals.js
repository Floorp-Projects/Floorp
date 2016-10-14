// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

const { Instance, Module } = WebAssembly;

// Locally-defined globals
assertErrorMessage(() => wasmEvalText(`(module (global))`), SyntaxError, /parsing/);
assertErrorMessage(() => wasmEvalText(`(module (global i32))`), SyntaxError, /parsing/);
assertErrorMessage(() => wasmEvalText(`(module (global (mut i32)))`), SyntaxError, /parsing/);

// Initializer expressions.
wasmFailValidateText(`(module (global i32 (f32.const 13.37)))`, /type mismatch/);
wasmFailValidateText(`(module (global f64 (f32.const 13.37)))`, /type mismatch/);
wasmFailValidateText(`(module (global i32 (i32.add (i32.const 13) (i32.const 37))))`, /failed to read end/);

wasmFailValidateText(`(module (global i32 (get_global 0)))`, /out of range/);
wasmFailValidateText(`(module (global i32 (get_global 1)) (global i32 (i32.const 1)))`, /out of range/);

// Test a well-defined global section.
function testInner(type, initialValue, nextValue, coercion, assertFunc = assertEq)
{
    var module = wasmEvalText(`(module
        (global (mut ${type}) (${type}.const ${initialValue}))
        (global ${type} (${type}.const ${initialValue}))

        (func $get (result ${type}) (get_global 0))
        (func $set (param ${type}) (set_global 0 (get_local 0)))

        (func $get_cst (result ${type}) (get_global 1))

        (export "get" $get)
        (export "get_cst" $get_cst)

        (export "set" $set)
    )`).exports;

    assertFunc(module.get(), coercion(initialValue));
    assertEq(module.set(coercion(nextValue)), undefined);
    assertFunc(module.get(), coercion(nextValue));

    assertFunc(module.get_cst(), coercion(initialValue));
}

testInner('i32', 13, 37, x => x|0);
testInner('f32', 13.37, 0.1989, Math.fround);
testInner('f64', 13.37, 0.1989, x => +x);

// Semantic errors.
wasmFailValidateText(`(module (global (mut i32) (i32.const 1337)) (func (set_global 1 (i32.const 0))))`, /out of range/);
wasmFailValidateText(`(module (global i32 (i32.const 1337)) (func (set_global 0 (i32.const 0))))`, /can't write an immutable global/);

// Big module with many variables: test that setting one doesn't overwrite the
// other ones.
function get_set(i, type) {
    return `
        (func $get_${i} (result ${type}) (get_global ${i}))
        (func $set_${i} (param ${type}) (set_global ${i} (get_local 0)))
    `;
}

var module = wasmEvalText(`(module
    (global (mut i32) (i32.const 42))
    (global (mut i32) (i32.const 10))
    (global (mut f32) (f32.const 13.37))
    (global (mut f64) (f64.const 13.37))
    (global (mut i32) (i32.const -18))

    ${get_set(0, 'i32')}
    ${get_set(1, 'i32')}
    ${get_set(2, 'f32')}
    ${get_set(3, 'f64')}
    ${get_set(4, 'i32')}

    (export "get0" $get_0) (export "set0" $set_0)
    (export "get1" $get_1) (export "set1" $set_1)
    (export "get2" $get_2) (export "set2" $set_2)
    (export "get3" $get_3) (export "set3" $set_3)
    (export "get4" $get_4) (export "set4" $set_4)
)`).exports;

let values = [42, 10, Math.fround(13.37), 13.37, -18];
let nextValues = [13, 37, Math.fround(-17.89), 9.3, -13];
for (let i = 0; i < 5; i++) {
    assertEq(module[`get${i}`](), values[i]);
    assertEq(module[`set${i}`](nextValues[i]), undefined);
    assertEq(module[`get${i}`](), nextValues[i]);
    for (let j = 0; j < 5; j++) {
        if (i === j)
            continue;
        assertEq(module[`get${j}`](), values[j]);
    }
    assertEq(module[`set${i}`](values[i]), undefined);
    assertEq(module[`get${i}`](), values[i]);
}

// Initializer expressions can also be used in elem section initializers.
wasmFailValidateText(`(module (import "globals" "a" (global f32)) (table 4 anyfunc) (elem (get_global 0) $f) (func $f))`, /type mismatch/);

module = wasmEvalText(`(module
    (import "globals" "a" (global i32))
    (table (export "tbl") 4 anyfunc)
    (elem (get_global 0) $f)
    (func $f)
    (export "f" $f)
)`, {
    globals: {
        a: 1
    }
}).exports;
assertEq(module.f, module.tbl.get(1));

// Import/export rules.
wasmFailValidateText(`(module (import "globals" "x" (global (mut i32))))`, /can't import.* mutable globals in the MVP/);
wasmFailValidateText(`(module (global (mut i32) (i32.const 42)) (export "" global 0))`, /can't .*export mutable globals in the MVP/);

// Import/export semantics.
module = wasmEvalText(`(module
 (import $g "globals" "x" (global i32))
 (func $get (result i32) (get_global $g))
 (export "getter" $get)
 (export "value" global 0)
)`, { globals: {x: 42} }).exports;

assertEq(module.getter(), 42);
assertEq(module.value, 42);

// Can only import numbers (no implicit coercions).
module = new WebAssembly.Module(wasmTextToBinary(`(module
    (global (import "globs" "i32") i32)
    (global (import "globs" "f32") f32)
    (global (import "globs" "f64") f32)
)`));

const assertLinkFails = (m, imp, err) => {
    assertErrorMessage(() => new WebAssembly.Instance(m, imp), TypeError, err);
}

var imp = {
    globs: {
        i32: 0,
        f32: Infinity,
        f64: NaN
    }
};

let i = new WebAssembly.Instance(module, imp);

for (let v of [
    null,
    {},
    "42",
    /not a number/,
    false,
    undefined,
    Symbol(),
    { valueOf() { return 42; } }
]) {
    imp.globs.i32 = v;
    assertLinkFails(module, imp, /not a number/);

    imp.globs.i32 = 0;
    imp.globs.f32 = v;
    assertLinkFails(module, imp, /not a number/);

    imp.globs.f32 = Math.fround(13.37);
    imp.globs.f64 = v;
    assertLinkFails(module, imp, /not a number/);

    imp.globs.f64 = 13.37;
}

// Imported globals and locally defined globals use the same index space.
module = wasmEvalText(`(module
 (import "globals" "x" (global i32))
 (global i32 (i32.const 1337))
 (export "imported" global 0)
 (export "defined" global 1)
)`, { globals: {x: 42} }).exports;

assertEq(module.imported, 42);
assertEq(module.defined, 1337);

// Initializer expressions can reference an imported immutable global.
wasmFailValidateText(`(module (global f32 (f32.const 13.37)) (global i32 (get_global 0)))`, /must reference a global immutable import/);
wasmFailValidateText(`(module (global (mut f32) (f32.const 13.37)) (global i32 (get_global 0)))`, /must reference a global immutable import/);
wasmFailValidateText(`(module (global (mut i32) (i32.const 0)) (global i32 (get_global 0)))`, /must reference a global immutable import/);

wasmFailValidateText(`(module (import "globals" "a" (global f32)) (global i32 (get_global 0)))`, /type mismatch/);

function testInitExpr(type, initialValue, nextValue, coercion, assertFunc = assertEq) {
    var module = wasmEvalText(`(module
        (import "globals" "a" (global ${type}))

        (global (mut ${type}) (get_global 0))
        (global ${type} (get_global 0))

        (func $get0 (result ${type}) (get_global 0))

        (func $get1 (result ${type}) (get_global 1))
        (func $set1 (param ${type}) (set_global 1 (get_local 0)))

        (func $get_cst (result ${type}) (get_global 2))

        (export "get0" $get0)
        (export "get1" $get1)
        (export "get_cst" $get_cst)

        (export "set1" $set1)
    )`, {
        globals: {
            a: coercion(initialValue)
        }
    }).exports;

    assertFunc(module.get0(), coercion(initialValue));
    assertFunc(module.get1(), coercion(initialValue));

    assertEq(module.set1(coercion(nextValue)), undefined);
    assertFunc(module.get1(), coercion(nextValue));
    assertFunc(module.get0(), coercion(initialValue));

    assertFunc(module.get_cst(), coercion(initialValue));
}

testInitExpr('i32', 13, 37, x => x|0);
testInitExpr('f32', 13.37, 0.1989, Math.fround);
testInitExpr('f64', 13.37, 0.1989, x => +x);

// Int64.
{
    wasmFailValidateText(`(module (import "globals" "x" (global i64)))`, /can't import.* an Int64 global/);
    wasmFailValidateText(`(module (global i64 (i64.const 42)) (export "" global 0))`, /can't .*export an Int64 global/);

    setJitCompilerOption('wasm.test-mode', 1);
    testInner('i64', '0x531642753864975F', '0x123456789abcdef0', createI64, assertEqI64);
    testInitExpr('i64', '0x531642753864975F', '0x123456789abcdef0', createI64, assertEqI64);

    module = wasmEvalText(`(module
     (import "globals" "x" (global i64))
     (global i64 (i64.const 0xFAFADADABABA))
     (export "imported" global 0)
     (export "defined" global 1)
    )`, { globals: {x: createI64('0x1234567887654321')} }).exports;

    assertEqI64(module.imported, createI64('0x1234567887654321'));
    assertEqI64(module.defined, createI64('0xFAFADADABABA'));

    setJitCompilerOption('wasm.test-mode', 0);
}
