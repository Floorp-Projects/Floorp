// |jit-test| exitstatus: 3; skip-if: !wasmIsSupported()

let { exports } = new WebAssembly.Instance(
    new WebAssembly.Module(wasmTextToBinary(`
    (module
        (func (export "f") (param i32) (param i32) (param i32) (param i32) (result i32)
            get_local 0
            i32.popcnt
            get_local 1
            i32.popcnt
            i32.add

            get_local 2
            i32.popcnt
            get_local 3
            i32.popcnt
            i32.add

            get_local 0
            get_local 1
            i32.sub

            get_local 2
            get_local 3
            i32.sub

            get_local 0
            get_local 1
            i32.mul

            get_local 2
            get_local 3
            i32.mul

            get_local 0
            get_local 2
            i32.sub

            get_local 1
            get_local 3
            i32.sub

            get_local 0
            get_local 1
            i32.add

            get_local 2
            get_local 3
            i32.add

            get_local 0
            get_local 2
            i32.add

            get_local 1
            get_local 3
            i32.add

            get_local 0
            i32.ctz
            get_local 1
            i32.ctz
            i32.add

            get_local 2
            i32.ctz
            get_local 3
            i32.ctz

            get_local 0
            get_local 1
            get_local 2
            get_local 3
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
