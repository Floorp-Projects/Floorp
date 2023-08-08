// |jit-test| skip-if: !wasmGcEnabled(); --wasm-test-serialization

// Test that serialization doesn't create a forward reference to the third
// struct when serializing the reference to the first struct, which is
// structurally identical to the first struct.
wasmEvalText(`(module
  (type (;0;) (struct))
  (type (;2;) (struct (field (ref 0))))
  (type (;3;) (struct))
)`);
