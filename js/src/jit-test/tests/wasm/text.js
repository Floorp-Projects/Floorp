load(libdir + "wasm.js");

var parsingError = /parsing wasm text at/;

assertErrorMessage(() => wasmEvalText(''), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('('), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(m'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(moduler'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func) (export "a'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func (local $a i32) (param $b f32)))'), SyntaxError, parsingError);

assertErrorMessage(() => wasmEvalText('(module (func $a) (func) (export "a" $a) (export "b" $b))'), SyntaxError, /function not found/);
assertErrorMessage(() => wasmEvalText('(module (import $foo "a" "b") (import $foo "a" "b"))'), SyntaxError, /duplicate import/);
assertErrorMessage(() => wasmEvalText('(module (func $foo) (func $foo))'), SyntaxError, /duplicate function/);
assertErrorMessage(() => wasmEvalText('(module (func (param $a i32) (local $a i32)))'), SyntaxError, /duplicate var/);
assertErrorMessage(() => wasmEvalText('(module (func (get_local $a)))'), SyntaxError, /local not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (type $a (func (param i32))))'), SyntaxError, /duplicate signature/);
assertErrorMessage(() => wasmEvalText('(module (import "a" "") (func (call_import $abc)))'), SyntaxError, /import not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (func (type $b) (i32.const 13)))'), SyntaxError, /signature not found/);
assertErrorMessage(() => wasmEvalText('(module (type $a (func)) (func (call_indirect $c (get_local 0) (i32.const 0))))'), SyntaxError, /signature not found/);
assertErrorMessage(() => wasmEvalText('(module (func (br $a)))'), SyntaxError, /label not found/);
assertErrorMessage(() => wasmEvalText('(module (func (block $a) (br $a)))'), SyntaxError, /label not found/);

// Note: the s-expression text format is temporary, this file is mostly just to
// hold basic error smoke tests.
