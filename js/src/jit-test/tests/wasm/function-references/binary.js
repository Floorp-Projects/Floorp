// |jit-test| skip-if: !wasmFunctionReferencesEnabled()

load(libdir + "wasm-binary.js");

const v2vSig = {args:[], ret:VoidCode};
const v2vSigSection = sigSection([v2vSig]);

function checkInvalid(binary, errorMessage) {
    assertErrorMessage(() => new WebAssembly.Module(binary),
        WebAssembly.CompileError,
        errorMessage);
}

// The immediate of ref.null is a heap type, not a general reference type

const invalidRefNullHeapBody = moduleWithSections([
    v2vSigSection,
    declSection([0]),
    bodySection([
        funcBody({locals:[], body:[
            RefNullCode,
            OptRefCode,
            AnyFuncCode,
            DropCode,
        ]})
    ])
]);
checkInvalid(invalidRefNullHeapBody, /invalid heap type/);

const invalidRefNullHeapElem = moduleWithSections([
    generalElemSection([
        {
            flag: PassiveElemExpr,
            typeCode: AnyFuncCode,
            elems: [
                [RefNullCode, OptRefCode, AnyFuncCode, EndCode]
            ]
        }
    ])
]);
checkInvalid(invalidRefNullHeapElem, /invalid heap type/);

const invalidRefNullHeapGlobal = moduleWithSections([
    globalSection([
        {
            valType: AnyFuncCode,
            flag: 0,
            initExpr: [RefNullCode, OptRefCode, AnyFuncCode, EndCode]
        }
    ])
]);
checkInvalid(invalidRefNullHeapGlobal, /invalid heap type/);
