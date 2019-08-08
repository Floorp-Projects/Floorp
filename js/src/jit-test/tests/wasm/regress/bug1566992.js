let { exports: { f } } = wasmEvalText(`
(module
    (func (export "f")
        (block
            i64.const 0xfffffffe00000000
            i32.wrap_i64
            br_table 0 1
        )
        unreachable
    )
)
`);

assertErrorMessage(f, WebAssembly.RuntimeError, /unreachable/);
