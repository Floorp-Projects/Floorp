load(libdir + "wasm.js");

if (!this.wasmEval)
    quit();

var parsingError = /parsing wasm text at/;

assertErrorMessage(() => wasmEvalText(""), Error, parsingError);
assertErrorMessage(() => wasmEvalText("("), Error, parsingError);
assertErrorMessage(() => wasmEvalText("(m"), Error, parsingError);
assertErrorMessage(() => wasmEvalText("(module"), Error, parsingError);
assertErrorMessage(() => wasmEvalText("(moduler"), Error, parsingError);

// Note: the s-expression text format is temporary, this file is mostly just to
// hold basic error smoke tests.
