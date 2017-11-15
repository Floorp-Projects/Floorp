for (let type of ['i32', 'f32', 'f64']) {
    assertEq(wasmEvalText(`
        (module
         (func $test (result ${type}) (param $p ${type}) (param $p2 ${type})
            get_local $p
            get_local $p2
            (block)
            drop
         )
         (export "test" $test)
        )
    `).exports.test(0x1337abc0, 0xffffffff), 0x1337abc0);
}

assertEq(wasmEvalText(`
    (module
     (func $test (result i32) (param $p i32) (param $p2 f32) (param $p3 f64) (param $p4 i32)
        get_local $p
        get_local $p2
        get_local $p3
        get_local $p4
        (block)
        drop
        (block)
        (block)
        drop
        drop
     )
     (export "test" $test)
    )
`).exports.test(0x1337abc0, 0xffffffff), 0x1337abc0);

wasmAssert(`
    (module
     (func $test (result i64) (param $p i64) (param $p2 i64)
        get_local $p
        get_local $p2
        (block)
        drop
     )
     (export "test" $test)
    )
`, [
    { type: 'i64', func: '$test', args: ['(i64.const 0x1337abc0)', '(i64.const -1)'], expected: '0x1337abc0' }
]);
