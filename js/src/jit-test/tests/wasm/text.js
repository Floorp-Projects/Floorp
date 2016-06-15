// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

var parsingError = /parsing wasm text at/;

assertErrorMessage(() => wasmEvalText(''), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('('), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(m'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(moduler'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func) (export "a'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func (local $a i32) (param $b f32)))'), SyntaxError, parsingError);

assertErrorMessage(() => wasmEvalText('(module (func $a) (func) (export "a" $a) (export "b" $b))'), SyntaxError, /function label '\$b' not found/);
assertErrorMessage(() => wasmEvalText('(module (import $foo "a" "b") (import $foo "a" "b"))'), SyntaxError, /duplicate import/);
assertErrorMessage(() => wasmEvalText('(module (func $foo) (func $foo))'), SyntaxError, /duplicate function/);
assertErrorMessage(() => wasmEvalText('(module (func (param $a i32) (local $a i32)))'), SyntaxError, /duplicate var/);
assertErrorMessage(() => wasmEvalText('(module (func (get_local $a)))'), SyntaxError, /local label '\$a' not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (type $a (func (param i32))))'), SyntaxError, /duplicate signature/);
assertErrorMessage(() => wasmEvalText('(module (import "a" "") (func (call_import $abc)))'), SyntaxError, /import label '\$abc' not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (func (type $b) (i32.const 13)))'), SyntaxError, /signature label '\$b' not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (func (call_indirect $c (get_local 0) (i32.const 0))))'), SyntaxError, /signature label '\$c' not found/);
assertErrorMessage(() => wasmEvalText('(module (func (br $a)))'), SyntaxError, /branch target label '\$a' not found/);
assertErrorMessage(() => wasmEvalText('(module (func (block $a) (br $a)))'), SyntaxError, /branch target label '\$a' not found/);

wasmEvalText('(module (func (param $a i32)))');
wasmEvalText('(module (func (param i32)))');
wasmEvalText('(module (func (param i32 i32 f32 f64 i32)))');
assertErrorMessage(() => wasmEvalText('(module (func (param $a)))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func (param $a i32 i32)))'), SyntaxError, parsingError);

wasmEvalText('(module (func (local $a i32)))');
wasmEvalText('(module (func (local i32)))');
wasmEvalText('(module (func (local i32 i32 f32 f64 i32)))');
assertErrorMessage(() => wasmEvalText('(module (func (local $a)))'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func (local $a i32 i32)))'), SyntaxError, parsingError);

// Note: the s-expression text format is temporary, this file is mostly just to
// hold basic error smoke tests.
