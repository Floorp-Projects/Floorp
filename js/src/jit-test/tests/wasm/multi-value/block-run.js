wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.add
        (block (result i32 i32)
          (i32.const 32)
          (i32.const 10)))))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32) (i32.const 10)
      (block (param i32 i32) (result i32 i32))
      (i32.add)))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (block (param i32) (result i32 i32)
        (i32.const 10))
      (i32.add)))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.add
        (loop (result i32 i32)
          (i32.const 32)
          (i32.const 10)))))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32) (i32.const 10)
      (loop (param i32 i32) (result i32 i32))
      (i32.add)))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (loop (param i32) (result i32 i32)
        (i32.const 10))
      (i32.add)))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.add
        (block (result i32 i32)
          (i32.const 32)
          (i32.const 10)
          (br 0)))))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (block (param i32 i32) (result i32 i32)
        (br 0))
      (i32.add)))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (block (param i32) (result i32 i32)
        (i32.const 10)
        (br 0))
      (i32.add)))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.add
        (block (result i32 i32)
          (i32.const 32)
          (i32.const 10)
          (block (param i32 i32) (result i32 i32)
            (br 1))))))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (block (param i32 i32) (result i32 i32)
        (block (param i32 i32) (result i32 i32)
          (br 1)))
      (i32.add)))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (block (param i32 i32) (result i32 i32)
        (block (param i32 i32) (result i32 i32)
          (br 0)))
      (i32.add)))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (block (param i32) (result i32 i32)
        (i32.const 10)
        (block (param i32 i32) (result i32 i32)
          (br 1)))
      (i32.add)))`,
            42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 1)
      (if (param i32 i32) (result i32)
          (then (i32.add))
          (else (i32.sub)))))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 1)
      (if (param i32 i32) (result i32 i32)
          (then
            (drop)
            (drop)
            (i32.const 10)
            (i32.const 32)))
      (i32.sub)))`,
             -22);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 0)
      (if (param i32 i32) (result i32 i32)
          (then
            (drop)
            (drop)
            (i32.const 10)
            (i32.const 32)))
      (i32.sub)))`,
             22);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 1)
      (if (param i32 i32) (result i32 i32)
          (then (return)))
      (i32.sub)))`,
             10);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 0)
      (if (param i32 i32) (result i32 i32)
          (then (return)))
      (i32.sub)))`,
             22);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 1)
      (if (param i32 i32) (result i32 i32)
          (then)
          (else (return)))
      (i32.sub)))`,
             22);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (i32.const 32)
      (i32.const 10)
      (i32.const 0)
      (if (param i32 i32) (result i32 i32)
          (then)
          (else (return)))
      (i32.sub)))`,
             10);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (local $i i32)
      (i32.const 0) ;; sum
      (i32.const 10) ;; i
      (loop $loop (param i32 i32) (result i32)
        (local.tee $i)
        (i32.add) ;; sum = i + sum
        (i32.sub (local.get $i) (i32.const 1))
        (i32.eqz (local.tee $i))
        (if (param i32) (result i32)
            (then)
            (else (local.get $i) (br $loop))))))`,
            55);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (local i32)
      i32.const 42
      local.get 0
      local.get 0
      loop (param i32 i32 i32) (result i32)
        drop
        drop
      end))`,
             42);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (block (result i32 i32)
        (i32.popcnt (i32.const 1))
        (i32.popcnt (i32.const 3))
        (block (param i32 i32)
          (i32.const 6)
          (i32.const 7)
          (br 1))
        (unreachable))
      i32.add))`,
             13);

wasmFullPass(`
  (module
    (func (export "run") (result i32)
      (block (result i32 i32)
        (i32.popcnt (i32.const 1))
        (i32.popcnt (i32.const 3))
        (block) ;; sync()
        (i32.const 6)
        (i32.const 7)
        (br 0))
      i32.add))`,
             13);
