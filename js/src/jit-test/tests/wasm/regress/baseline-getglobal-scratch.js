new WebAssembly.Module(wasmTextToBinary(`
(module
    (global $g (mut i32) (i32.const 42))
    (func (param $i i32)
        get_local $i
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        get_global $g
        unreachable
    )
)
`))
