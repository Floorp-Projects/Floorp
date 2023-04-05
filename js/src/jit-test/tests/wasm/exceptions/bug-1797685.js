assertErrorMessage(() => wasmEvalText(`(module
  (func)
  (func
    try
      call 0
    delegate 4294967295
  )
)`), WebAssembly.CompileError, /delegate/);
