// Bug 1319415

load(libdir + "wasm.js");

var src =
`(module
  (func (result i32)
    i32.const 0
    i32.const 1
    br_if 0
    unreachable)
  (export "run" 0))`;

wasmFullPass(src, 0);
