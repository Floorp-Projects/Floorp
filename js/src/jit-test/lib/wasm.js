if (!wasmIsSupported())
    quit();

load(libdir + "asserts.js");

function wasmEvalText(str, imports) {
    return Wasm.instantiateModule(wasmTextToBinary(str), imports);
}

function mismatchError(actual, expect) {
    var str = `type mismatch: expression has type ${actual} but expected ${expect}`;
    return RegExp(str);
}
