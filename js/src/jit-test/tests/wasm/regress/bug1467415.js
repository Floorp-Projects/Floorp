
// This exposed an alias analysis bug in the Wasm Ion machinery.  It should
// compute 125 (88 + 37) but with the bug would compute 176 (88 + 88) instead.
// See bug 1467415.

let mt = `
(module
  (memory 1 1)
  (data (i32.const 0) "\\01\\00\\00\\00\\01\\00\\00\\00\\01\\00\\00\\00")
  (import "m" "g" (global (mut i32)))
  (import "m" "h" (global (mut i32)))
  (func (export "f") (result i32)
    (local i32)
    (local i32)
    (block i32
      (set_local 0 (get_global 0))
      (block i32
        (set_global 1 (i32.const 37))
        (block i32
          (set_local 1 (get_global 0))
          (i32.add (get_local 0) (get_local 1)))))))
`;

let glob = new WebAssembly.Global({value:'i32', mutable:true}, 88);
let module = new WebAssembly.Module(wasmTextToBinary(mt));
let ins = new WebAssembly.Instance(module, {m:{g:glob, h:glob}});

let shouldBe125 = ins.exports.f();
assertEq(shouldBe125, 125);
