if (!wasmGcEnabled()) {
    quit(0);
}

load(libdir + "wasm-binary.js");

const invalidRefNullBody = funcBody({locals:[], body:[
    RefNull,
    RefCode,
    0x42,

    RefNull,
    RefCode,
    0x10,

    // Condition code;
    I32ConstCode,
    0x10,

    SelectCode,
    DropCode
]});

const v2vSig = {args:[], ret:VoidCode};
const v2vSigSection = sigSection([v2vSig]);

assertErrorMessage(() => new WebAssembly.Module(moduleWithSections([v2vSigSection, declSection([0]), bodySection([invalidRefNullBody])])), WebAssembly.CompileError, /invalid nullref type/);
