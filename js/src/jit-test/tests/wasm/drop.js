for (let type of ['i32', 'f32', 'f64']) {
    assertEq(wasmEvalText(`
        (module
         (func $test (param $p ${type}) (param $p2 ${type}) (result ${type})
            local.get $p
            local.get $p2
            (block)
            drop
         )
         (export "test" (func $test))
        )
    `).exports.test(0x1337abc0, 0xffffffff), 0x1337abc0);
}

assertEq(wasmEvalText(`
    (module
     (func $test (param $p i32) (param $p2 f32) (param $p3 f64) (param $p4 i32) (result i32)
        local.get $p
        local.get $p2
        local.get $p3
        local.get $p4
        (block)
        drop
        (block)
        (block)
        drop
        drop
     )
     (export "test" (func $test))
    )
`).exports.test(0x1337abc0, 0xffffffff), 0x1337abc0);

wasmAssert(`
    (module
     (func $test (param $p i64) (param $p2 i64) (result i64)
        local.get $p
        local.get $p2
        (block)
        drop
     )
     (export "test" (func $test))
    )
`, [
    { type: 'i64', func: '$test', args: ['(i64.const 0x1337abc0)', '(i64.const -1)'], expected: '0x1337abc0' }
]);
