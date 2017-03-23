if (typeof oomTest === 'undefined')
    quit();

try {
    oomTest(Function(`
        new WebAssembly.Module(wasmTextToBinary(\`
            (module (func (result i32) (param f64) (param f32)
                i64.const 0
                get_local 0
                drop
                i32.wrap/i64
                f64.const 0
                f64.const 0
                i32.const 0
                select
                f32.const 0
                f32.const 0
                f32.const 0
                i32.const 0
                select
                i32.const 0
                i32.const 0
                i32.const 0
                select
                select
                drop
                drop
            ))
        \`))
    `));
} catch(e) { }
