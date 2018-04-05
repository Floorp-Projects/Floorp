if (!this.gczeal || !WebAssembly.Global)
    quit();

gczeal(9, 10);
function wasmEvalText(str, imports) {
    let binary = wasmTextToBinary(str);
    m = new WebAssembly.Module(binary);
    return new WebAssembly.Instance(m, imports);
}
assertEq(wasmEvalText(`(module
    (global (import "a" "b") (mut i32))
    (func (export "get") (result i32) get_global 0)
)`, {  a: { b: 42 }}).exports.get(), 42);
for (let v of []) {}
function testInitExpr(type, initialValue, nextValue, coercion, assertFunc = assertEq) {
    var module = wasmEvalText(`(module
        (import "globals" "a" (global ${type}))
        (global $glob_imm ${type} (get_global 0))
        (export "global_imm" (global $glob_imm))
    )`, {
        globals: {
            a: coercion(initialValue)
        }
    }).exports;
}
testInitExpr('i32', 13, 37, x => x | 0);
