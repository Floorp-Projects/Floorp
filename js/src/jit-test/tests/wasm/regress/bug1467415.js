
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
      (local.set 0 (global.get 0))
      (block i32
        (global.set 1 (i32.const 37))
        (block i32
          (local.set 1 (global.get 0))
          (i32.add (local.get 0) (local.get 1)))))))
`;

let glob = new WebAssembly.Global({value:'i32', mutable:true}, 88);
let module = new WebAssembly.Module(wasmTextToBinary(mt));
let ins = new WebAssembly.Instance(module, {m:{g:glob, h:glob}});

let shouldBe125 = ins.exports.f();
assertEq(shouldBe125, 125);
