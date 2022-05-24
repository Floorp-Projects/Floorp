// Dead code elimination can remove wasm calls that may leave behind dangling
// try notes.
wasmEvalText(`(module
  (type (;0;) (func (result i32 i32 i32)))
  (func $main (type 0) (result i32 i32 i32)
    try (result i32 i32 i32)  ;; label = @1
      call $main
      call $main
      i32.const 541
      i32.const 0
      br_if 0 (;@1;)
      br_if 0 (;@1;)
      br_if 0 (;@1;)
      br_if 0 (;@1;)
      call $main
      call $main
      br_if 0 (;@1;)
      br_if 0 (;@1;)
      call $main
      call $main
      call $main
      unreachable
    end
    unreachable
  )
)`);
