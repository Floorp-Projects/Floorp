// |jit-test| skip-if: !wasmReftypesEnabled()

load(libdir + "wasm-binary.js");

const v2vSig = {args:[], ret:VoidCode};
const v2vSigSection = sigSection([v2vSig]);

function checkInvalid(body, errorMessage) {
    assertErrorMessage(() => new WebAssembly.Module(
        moduleWithSections([v2vSigSection, declSection([0]), bodySection([body])])),
                       WebAssembly.CompileError,
                       errorMessage);
}

const invalidRefBlockType = funcBody({locals:[], body:[
    BlockCode,
    OptRefCode,
    0x42,
    EndCode,
]});
checkInvalid(invalidRefBlockType, /ref/);

const invalidTooBigRefType = funcBody({locals:[], body:[
    BlockCode,
    OptRefCode,
    varU32(1000000),
    EndCode,
]});
checkInvalid(invalidTooBigRefType, /ref/);
