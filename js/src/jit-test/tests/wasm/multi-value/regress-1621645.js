wasmFullPass(`
(module
  (func $f (result i64 i64 i64 i64 i64
                   i64 i64 i64 i64 i64)
    (i64.const 0)
    (i64.const 1)
    (i64.const 2)
    (i64.const 3)
    (i64.const 4)
    (i64.const 5)
    (i64.const 6)
    (i64.const 7)
    (i64.const 8)
    (i64.const 9))
  (func (export "run") (result i32)
    (call $f)
    (i64.add) (i64.add) (i64.add) (i64.add) (i64.add)
    (i64.add) (i64.add) (i64.add) (i64.add)
    (i32.wrap_i64)))`,
            45);
