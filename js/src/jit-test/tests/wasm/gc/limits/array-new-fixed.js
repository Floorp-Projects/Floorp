// |jit-test| --setpref=wasm_gc; include:wasm.js;

// array.new_fixed has limit of 10_000 operands
wasmFailValidateText(`(module
  (type $a (array i32))
  (func
    array.new_fixed $a 10001
  )
)`, /too many/);
