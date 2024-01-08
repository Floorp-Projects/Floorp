let {a} = wasmEvalText(`(module
  (tag (param ${'i32 '.repeat(10)}))

  (func (result ${'i32 '.repeat(130)})
    unreachable
  )
  (func (export "a")
    call 0
	try
	catch 0
		unreachable
	end
    unreachable
  )
)
`).exports;
assertErrorMessage(a, WebAssembly.RuntimeError, /unreachable/);
