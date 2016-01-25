load(libdir + "asserts.js");

function wasmEvalText(str, imports) {
    return wasmEval(wasmTextToBinary(str), imports);
}
