load(libdir + "wasm.js");

if (!wasmIsSupported())
    quit();

function mismatchError(actual, expect) {
    var str = "type mismatch: expression has type " + actual + " but expected " + expect;
    return RegExp(str);
}

assertEq(wasmEvalText('(module (func (param i32) (param i32) (result i32) (i32.add (get_local 0) (get_local 1))) (export "" 0))')(2, 40), 42);

assertErrorMessage(() => wasmEvalText('(module (func (param f32) (param i32) (result i32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (result i32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param i32) (result f32) (i32.add (get_local 0) (get_local 1))))'), TypeError, mismatchError("i32", "f32"));
