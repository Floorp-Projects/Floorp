new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
    (func (param i64))
    (func (export "f")
        i64.const 2
        i64.const -9223372036854775808
        i64.mul
        call 0
    )
)
`))).exports.f();
