// |jit-test| exitstatus: 6; skip-if: !wasmIsSupported()

const {Module, Instance} = WebAssembly;

const {innerWasm} = new Instance(new Module(wasmTextToBinary(`(module
    (func (export "innerWasm") (result i32)
        (local i32)
        (loop $top
            (local.set 0 (i32.add (local.get 0) (i32.const 1)))
            (br_if $top (i32.lt_u (local.get 0) (i32.const 100000)))
        )
        (local.get 0)
    )
)`))).exports;

function middleJS() {
    innerWasm();
}

const {outerWasm} = new Instance(new Module(wasmTextToBinary(`(module
    (func $middleJS (import "" "middleJS"))
    (func (export "outerWasm") (call $middleJS))
)`)), {'':{middleJS}}).exports;

timeout(1);
while (true) {
    outerWasm();
}
