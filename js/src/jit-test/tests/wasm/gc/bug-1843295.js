// |jit-test| skip-if: !wasmGcEnabled(); --wasm-test-serialization

wasmEvalText(`(module
  (type (sub (array (mut i32))))
  (type (sub 0 (array (mut i32))))
  (rec
    (type (sub (array (mut i32))))
    (type (sub 2 (array (mut i32))))
  )
)`);
