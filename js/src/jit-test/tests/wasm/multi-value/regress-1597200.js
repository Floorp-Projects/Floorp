new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (func (export "main") (result i32)
    (i32.const 1)
    (i32.const 2)
    (i32.const 3)
    (loop (param i32 i32 i32)
      (i32.popcnt)
      (i32.const -63)
      (br 0))
    (unreachable)))`)));

wasmFullPass(`
(module
  (func (export "run") (result i32)
    (block (result i32 i32 i32)
      (i32.const 41)
      (i32.const 42)
      (i32.const 43)
      (loop (param i32 i32 i32)
        (i32.eqz)
        (i32.const -63)
        (br 1))
      (unreachable))
    (drop)
    (drop)))`,
            42);

wasmFullPass(`
(module
  (func (export "run") (result i32)
    (block (result i32 i32 i32)
      (i32.popcnt (i32.const 0x0))
      (i32.popcnt (i32.const 0xf))
      (i32.popcnt (i32.const 0xff))
      (i32.popcnt (i32.const 0xfff))
      (block) ;; Force a sync().
      (br 0))
    (drop)
    (drop)))`,
             4);
