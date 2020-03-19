var module = new WebAssembly.Module(wasmTextToBinary(`
    (module
        (import "global" "func")
        (func (export "f")
         call 0
        )
    )
`));
new WebAssembly.Instance(module, { global: { func: enableGeckoProfiling } }).exports.f();
