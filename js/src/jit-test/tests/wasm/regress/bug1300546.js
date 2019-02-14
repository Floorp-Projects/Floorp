try {
    wasmEvalText(`
    (module
      (type $type0 (func))
      (func $func0
          (nop)
          (f64.load offset=59 align=1 (i32.const 0))
          (memory.size)
          (memory.size)
          (memory.size)
          (memory.size)
          (memory.size)
          (memory.size)
          (memory.size)
          (memory.size)
          (i64.rem_s (i64.const 17) (i64.xor (i64.const 17) (i64.xor (i64.const 17) (i64.xor (i64.xor (i64.const 17) (i64.const 17)) (i64.xor (i64.const 17) (i64.const 17))))))

         (i64.rem_s
          (i64.const 17)
          (i64.xor
            (i64.rem_s (i64.const 17) (i64.const 17))
            (i64.xor (i64.rem_s (i64.const 17) (i64.const 17)) (i64.xor (i64.const 17) (i64.const 17)))))
      )
      (memory 1 1)
    )
    `);
} catch(e) {
}
