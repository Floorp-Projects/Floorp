
// A simple test case for memory.copy between different memories.
// See bug 1861267.

let i = wasmEvalText(
`(module
  (memory $$mem0 (data "staubfaenger"))
  (memory $$mem1 (data "\\ee\\22\\55\\ff"))
  (memory $$mem2 (data "\\dd\\33\\66\\00"))
  (memory $$mem3 (data "schnickschnack"))

  (func (export "copy0to3") (param i32 i32 i32)
    (memory.copy $$mem3 $$mem0
      (local.get 0)
      (local.get 1)
      (local.get 2))
  )
  (func (export "copy3to0") (param i32 i32 i32)
    (memory.copy $$mem0 $$mem3
      (local.get 0)
      (local.get 1)
      (local.get 2))
  )

  (func (export "read0") (param i32) (result i32)
    (i32.load8_u $$mem0 (local.get 0))
  )
  (func (export "read3") (param i32) (result i32)
    (i32.load8_u $$mem3 (local.get 0))
  )
)`);

i.exports.copy0to3(4, 5, 6);
let s = "";
for (let ix = 0; ix < 14; ix++) {
    s = s + String.fromCharCode(i.exports.read3(ix));
}

i.exports.copy3to0(8, 2, 7);
s = s + "_";
for (let ix = 0; ix < 12; ix++) {
    s = s + String.fromCharCode(i.exports.read0(ix));
}

assertEq(s, "schnfaengenack_staubfaehnfa");
