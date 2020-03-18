wasmFullPass(`
  (module
    (func (result i32 i32)
      (i32.const 52)
      (i32.const 10))
    (func (export "run") (result i32)
      (call 0)
      i32.sub))`,
            42);

wasmFullPass(`
  (module
    (func (param i32 i32) (result i32 i32)
      (local.get 0)
      (local.get 1))
    (func (export "run") (result i32)
      (i32.const 52)
      (i32.const 10)
      (call 0)
      i32.sub))`,
            42);

wasmFullPass(`
  (module
    (func (param i32 i32 i32 i32 i32
                 i32 i32 i32 i32 i32)
          (result i32 i32)
      (local.get 8)
      (local.get 9))
    (func (export "run") (result i32)
      (i32.const 0)
      (i32.const 1)
      (i32.const 2)
      (i32.const 3)
      (i32.const 4)
      (i32.const 5)
      (i32.const 6)
      (i32.const 7)
      (i32.const 52)
      (i32.const 10)
      (call 0)
      i32.sub))`,
            42);

wasmFullPass(`
  (module
    (func (param i32 i32 i32 i32 i32
                 i32 i32 i32 i32 i32)
          (result i32 i32)
      (local i32 i32 i32 i32)
      (local.get 8)
      (local.get 9))
    (func (export "run") (result i32)
      (i32.const 0)
      (i32.const 1)
      (i32.const 2)
      (i32.const 3)
      (i32.const 4)
      (i32.const 5)
      (i32.const 6)
      (i32.const 7)
      (i32.const 52)
      (i32.const 10)
      (call 0)
      i32.sub))`,
            42);

wasmFullPass(`
  (module
    (func (param i32 i32 i32 i32 i32
                 i32 i32 i32 i32 i32)
          (result i32 i32 i32)
      (local i32 i32 i32 i32)
      (local.get 7)
      (local.get 8)
      (local.get 9))
    (func (export "run") (result i32)
      (i32.const 0)
      (i32.const 1)
      (i32.const 2)
      (i32.const 3)
      (i32.const 4)
      (i32.const 5)
      (i32.const 6)
      (i32.const 7)
      (i32.const 52)
      (i32.const 10)
      (call 0)
      i32.sub
      i32.sub))`,
            -35);

wasmFullPass(`
  (module
    (func (param i32 i64 i32 i64 i32
                 i64 i32 i64 i32 i64)
          (result i64 i32 i64)
      (local i32 i64 i32 i64)
      (local.get 7)
      (local.get 8)
      (local.get 9))
    (func (export "run") (result i32)
      (i32.const 0)
      (i64.const 1)
      (i32.const 2)
      (i64.const 3)
      (i32.const 4)
      (i64.const 5)
      (i32.const 6)
      (i64.const 7)
      (i32.const 52)
      (i64.const 10)
      (call 0)
      i32.wrap_i64
      i32.sub
      i64.extend_i32_s
      i64.sub
      i32.wrap_i64))`,
            -35);

