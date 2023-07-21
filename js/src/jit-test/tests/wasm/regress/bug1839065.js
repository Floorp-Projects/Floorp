// |jit-test| --wasm-gc; --wasm-function-references; skip-if: !wasmGcEnabled() || !wasmFunctionReferencesEnabled()
function wasmEvalText(str, imports) {
    let binary = wasmTextToBinary(str);
    m = new WebAssembly.Module(binary);
    return new WebAssembly.Instance(m, imports);
}
let { newElem, f1, f2, f3, f4 } = wasmEvalText(`
    (type $a (array funcref))
    (elem $e func $f1 $f2 $f3 $f4)
    (func $f1 )
    (func $f2 )
    (func $f3 )
    (func $f4 )
    (func (export "newElem") (result eqref)
             i32.const 0
             i32.const 4
            array.new_elem $a $e
    )`).exports;

let s = "no exception";
try {
    wasmGcReadField(newElem(), -1.5);
} catch (e) {
    s = "" + e;
}

assertEq(s, "Error: Second argument must be a non-negative integer. " +
            "Usage: wasmGcReadField(obj, index)");
