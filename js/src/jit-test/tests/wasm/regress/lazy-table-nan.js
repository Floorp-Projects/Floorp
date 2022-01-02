let i = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
    (func $f (result f32)
        f32.const nan:0x42
    )
    (table (export "table") 10 funcref)
    (elem (i32.const 0) $f)
)
`))).exports;
i.table.get(0)();
