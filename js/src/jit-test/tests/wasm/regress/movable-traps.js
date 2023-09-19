let bodies = [
    `
    i32.const 1
    i32.const 0
    i32.div_s
    `,
    `
    i32.const 1
    i32.const 0
    i32.rem_s
    `,
    `
    f64.const 1.7976931348623157e+308
    i64.trunc_f64_s
    `,
    `
    f32.const 3.40282347e+38
    i32.trunc_f32_s
    `
];

for (let body of bodies) {
    wasmFullPass(`
    (module
        (func $f (param $x i32) (result i32)
            loop $top (result i32)
                local.get $x
                if
                    local.get $x
                    br 2
                end
                ${body}
                br $top
            end
        )
        (export "run" (func $f))
    )`, 42, {}, 42);
}
