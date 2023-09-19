new WebAssembly.Module(wasmTextToBinary(`(module
    (memory 1)
    (func
        (i64.trunc_f32_s (f32.const 6.96875))
        (i32.load8_s (i32.const 0))
        (f32.const -7.66028056e-31)
        (unreachable)
    )
)`));
