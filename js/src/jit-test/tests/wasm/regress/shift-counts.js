load(libdir + "wasm.js");

// Bug 1280926, extracted from binary

wasmEvalText(
`(module
  (type $type0 (func (result i32)))
  (export "" $func0)
  (func $func0 (result i32)
   (i32.shr_s (i32.const -40) (i32.const 34))))`);
