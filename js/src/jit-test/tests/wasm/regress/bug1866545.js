// Tests stack alignment during tail calls (in Ion).

var ins = wasmEvalText(`
  (module
    (func $trap
        (param $i i32) (param $arr i32)
        unreachable
    )
    (func $second
      (return_call $trap
        (i32.const 33)
        (i32.const 66)
      )
    )
    (func (export "test")
        (call $second)
    )
  )
`);

assertErrorMessage(
    () => ins.exports.test(),
    WebAssembly.RuntimeError,
    "unreachable executed",
);
