let { exports } = new WebAssembly.Instance(
    new WebAssembly.Module(wasmTextToBinary(`
    (module
        (func (import "module" "fn") (param f64 i32) (result i32 f64))
        (func (export "f") (result i32)
            f64.const 4.2
            i32.const 7
            call 0
            drop
        )
    )
    `)),
    {
        "module": {
          fn(f32, i32) {
            assertEq(f32, 4.2);
            assertEq(i32, 7);
            return [2, 7.3];
          },
        }
    });

assertEq(exports.f(), 2);
