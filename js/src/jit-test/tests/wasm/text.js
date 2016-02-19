load(libdir + "wasm.js");

var parsingError = /parsing wasm text at/;

assertErrorMessage(() => wasmEvalText(''), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('('), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(m'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(moduler'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func) (export "a'), SyntaxError, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func (local $a i32) (param $b f32)))'), SyntaxError, parsingError);

// Note: the s-expression text format is temporary, this file is mostly just to
// hold basic error smoke tests.
