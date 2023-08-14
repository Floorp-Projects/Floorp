// |jit-test| exitstatus: 3; skip-if: !wasmIsSupported()

let { exports } = new WebAssembly.Instance(
    new WebAssembly.Module(wasmTextToBinary(`
    (module
        (func (export "f") (param i32) (param i32) (param i32) (param i32) (result i32)
            local.get 0
            i32.popcnt
            local.get 1
            i32.popcnt
            i32.add

            local.get 2
            i32.popcnt
            local.get 3
            i32.popcnt
            i32.add

            local.get 0
            local.get 1
            i32.sub

            local.get 2
            local.get 3
            i32.sub

            local.get 0
            local.get 1
            i32.mul

            local.get 2
            local.get 3
            i32.mul

            local.get 0
            local.get 2
            i32.sub

            local.get 1
            local.get 3
            i32.sub

            local.get 0
            local.get 1
            i32.add

            local.get 2
            local.get 3
            i32.add

            local.get 0
            local.get 2
            i32.add

            local.get 1
            local.get 3
            i32.add

            local.get 0
            i32.ctz
            local.get 1
            i32.ctz
            i32.add

            local.get 2
            i32.ctz
            local.get 3
            i32.ctz

            local.get 0
            local.get 1
            local.get 2
            local.get 3
            call 0

            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
            i32.add
        )
    )
    `))
);

timeout(1, function() {});
exports.f()
