setJitCompilerOption('wasm.test-mode', 1);

wasmFullPassI64(`
    (module
        (global (mut i64) (i64.const 9970292656026947164))
        (func (export "get_global_0") (result i64) get_global 0)

        (func (export "run") (result i64) (param i32)
            i64.const 8692897571457488645
            i64.const 1028567229461950342
            i64.mul
            get_global 0
            f32.const 3.141592653
            f32.floor
            f32.const -13.37
            f32.floor
            f32.copysign
            drop
            i64.div_u
        )
    )
`, createI64(1));
