// This attempts to test that we can do an 8-bit sign extend no matter what the
// source register.  This test is arguably somewhat tied to the baseline
// compiler's register allocator, but is still appropriate for most simple
// register allocators.  It works by filling an increasing number of registers
// with values so as eventually to force the source operand for extend8_s into a
// register that does not have a byte personality.

for ( let i=0; i < 8; i++) {
    let txt =
        `(module
           (func (export "f") (param i32) (result i32)
           ${adds(i)}
           (local.set 0 (i32.extend8_s (i32.add (local.get 0) (i32.const 1))))
           ${drops(i)}
           (local.get 0)))`;
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(txt)));
    assertEq(ins.exports.f(254), -1);
}

function adds(n) {
    let s = ""
    for ( let i=0; i < n; i++ )
        s += "(i32.add (local.get 0) (i32.const 1))\n";
    return s;
}

function drops(n) {
    let s = "";
    for ( let i=0; i < n; i++ )
        s += "drop\n";
    return s;
}


