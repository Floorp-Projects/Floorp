load(libdir + "wasm.js");

if (!wasmIsSupported())
    quit();

var parsingError = /parsing wasm text at/;

assertErrorMessage(() => wasmEvalText(''), Error, parsingError);
assertErrorMessage(() => wasmEvalText('('), Error, parsingError);
assertErrorMessage(() => wasmEvalText('(m'), Error, parsingError);
assertErrorMessage(() => wasmEvalText('(module'), Error, parsingError);
assertErrorMessage(() => wasmEvalText('(moduler'), Error, parsingError);
assertErrorMessage(() => wasmEvalText('(module (func) (export "a'), Error, parsingError);

// Note: the s-expression text format is temporary, this file is mostly just to
// hold basic error smoke tests.
