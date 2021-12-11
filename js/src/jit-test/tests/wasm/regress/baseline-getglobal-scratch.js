new WebAssembly.Module(wasmTextToBinary(`
(module
    (global $g (mut i32) (i32.const 42))
    (func (param $i i32)
        local.get $i
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        unreachable
    )
)
`));

new WebAssembly.Module(wasmTextToBinary(`
(module
    (global $g (mut i32) (i32.const 42))
    (func (param $i i32)
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        global.get $g
        local.get $i
        global.set $g
        unreachable
    )
)
`));
