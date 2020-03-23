// |jit-test| --no-wasm-bigint
//
// Tests code paths that are taken when BigInt/I64 conversion is disabled.

// Test i64 signature failures.

var f = wasmEvalText('(module (func (param i64) (result i32) (i32.const 123)) (export "" (func 0)))').exports[""];
assertErrorMessage(f, TypeError, /i64/);
var f = wasmEvalText('(module (func (param i32) (result i64) (i64.const 123)) (export "" (func 0)))').exports[""];
assertErrorMessage(f, TypeError, /i64/);

var f = wasmEvalText('(module (import $imp "a" "b" (param i64) (result i32)) (func $f (result i32) (call $imp (i64.const 0))) (export "" (func $f)))', {a:{b:()=>{}}}).exports[""];
assertErrorMessage(f, TypeError, /i64/);
var f = wasmEvalText('(module (import $imp "a" "b" (result i64)) (func $f (result i64) (call $imp)) (export "" (func $f)))', {a:{b:()=>{}}}).exports[""];
assertErrorMessage(f, TypeError, /i64/);

// Import and export related tests.
//
// i64 is disallowed when called from JS and will cause calls to fail before
// arguments are coerced (when BigInt/I64 conversion is off).

var sideEffect = false;
var i = wasmEvalText('(module (func (export "f") (param i64) (result i32) (i32.const 42)))').exports;
assertErrorMessage(() => i.f({ valueOf() { sideEffect = true; return 42; } }), TypeError, 'cannot pass i64 to or from JS');
assertEq(sideEffect, false);

i = wasmEvalText('(module (func (export "f") (param i32) (param i64) (result i32) (i32.const 42)))').exports;
assertErrorMessage(() => i.f({ valueOf() { sideEffect = true; return 42; } }, 0), TypeError, 'cannot pass i64 to or from JS');
assertEq(sideEffect, false);

i = wasmEvalText('(module (func (export "f") (param i32) (result i64) (i64.const 42)))').exports;
assertErrorMessage(() => i.f({ valueOf() { sideEffect = true; return 42; } }), TypeError, 'cannot pass i64 to or from JS');
assertEq(sideEffect, false);

i = wasmEvalText('(module (import "i64" "func" (param i64)) (export "f" (func 0)))', { i64: { func() {} } }).exports;
assertErrorMessage(() => i.f({ valueOf() { sideEffect = true; return 42; } }), TypeError, 'cannot pass i64 to or from JS');
assertEq(sideEffect, false);

i = wasmEvalText('(module (import "i64" "func" (param i32) (param i64)) (export "f" (func 0)))', { i64: { func() {} } }).exports;
assertErrorMessage(() => i.f({ valueOf() { sideEffect = true; return 42; } }, 0), TypeError, 'cannot pass i64 to or from JS');
assertEq(sideEffect, false);

i = wasmEvalText('(module (import "i64" "func" (result i64)) (export "f" (func 0)))', { i64: { func() {} } }).exports;
assertErrorMessage(() => i.f({ valueOf() { sideEffect = true; return 42; } }), TypeError, 'cannot pass i64 to or from JS');
assertEq(sideEffect, false);

// Global related tests.
//
// The test for a Number value dominates the guard against int64.
assertErrorMessage(() => wasmEvalText(`(module
                                        (import "globals" "x" (global i64)))`,
                                      {globals: {x:false}}),
                   LinkError,
                   /import object field 'x' is not a Number/);

// The imported value is a Number, so the int64 guard should stop us.
assertErrorMessage(() => wasmEvalText(`(module
                                        (import "globals" "x" (global i64)))`,
                                      {globals: {x:42}}),
                   LinkError,
                   /cannot pass i64 to or from JS/);

{
    // We can import and export i64 globals as cells.  They cannot be created
    // from JS because there's no way to specify a non-zero initial value; that
    // restriction is tested later.  But we can export one from a module and
    // import it into another.

    let i = wasmEvalText(`(module
                           (global (export "g") i64 (i64.const 37))
                           (global (export "h") (mut i64) (i64.const 37)))`);

    let j = wasmEvalText(`(module
                           (import "globals" "g" (global i64))
                           (func (export "f") (result i32)
                            (i64.eq (global.get 0) (i64.const 37))))`,
                         {globals: {g: i.exports.g}});

    assertEq(j.exports.f(), 1);

    // We cannot read or write i64 global values from JS.

    let g = i.exports.g;

    assertErrorMessage(() => i.exports.g.value, TypeError, /cannot pass i64 to or from JS/);

    // Mutability check comes before i64 check.
    assertErrorMessage(() => i.exports.g.value = 12, TypeError, /can't set value of immutable global/);
    assertErrorMessage(() => i.exports.h.value = 12, TypeError, /cannot pass i64 to or from JS/);
}

// WebAssembly.Global tests.
// These types should not work:
assertErrorMessage(() => new Global({}),                TypeError, /bad type for a WebAssembly.Global/);
assertErrorMessage(() => new Global({value: "fnord"}),  TypeError, /bad type for a WebAssembly.Global/);
assertErrorMessage(() => new Global(),                  TypeError, /Global requires at least 1 argument/);
assertErrorMessage(() => new Global({value: "i64"}, 0), TypeError, /bad type for a WebAssembly.Global/); // Initial value does not work

// Check against i64 import before matching mutability
let m = new Module(wasmTextToBinary(`(module
                                      (import "m" "g" (global (mut i64))))`));
assertErrorMessage(() => new Instance(m, {m: {g: 42}}),
                   LinkError,
                   i64Err);
