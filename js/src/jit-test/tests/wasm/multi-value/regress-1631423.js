wasmEvalText(`
    (module
      (func $main (export "main")
        (local i32)
        i32.const 1
        i32.const 2
        i32.const 3
        (loop (param i32 i32 i32)
          local.get 0
          i32.const 4
          i32.const 5
          i32.const 6
          i32.const 7
          br_if 0
          unreachable)))`);
