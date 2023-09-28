let memory = new WebAssembly.Memory({initial: 1});
let bytes = new Uint8Array(memory.buffer);

let module = wasmIntrinsicI8VecMul();
let instance = new WebAssembly.Instance(module, {
  "": {"memory": memory}
});
let {i8vecmul} = instance.exports;

// Test basic vector pairwise product
{
  // [0, 1, 2, 3] . [0, 2, 4, 6] = [0, 2, 8, 18]
  for (let i = 0; i < 4; i++) {
    bytes[i] = i;
    bytes[4 + i] = i * 2;
  }
  i8vecmul(
    /* dest */ 8,
    /* src1 */ 0,
    /* src2 */ 4,
    /* len */ 4);
  for (let i = 0; i < 4; i++) {
    assertEq(bytes[8 + i], i * i * 2);
  }
}

// Test bounds checking
{
  assertErrorMessage(() => i8vecmul(PageSizeInBytes - 1, 0, 0, 2), WebAssembly.RuntimeError, /index out of bounds/);
  assertErrorMessage(() => i8vecmul(0, PageSizeInBytes - 1, 0, 2), WebAssembly.RuntimeError, /index out of bounds/);
  assertErrorMessage(() => i8vecmul(0, 0, PageSizeInBytes - 1, 2), WebAssembly.RuntimeError, /index out of bounds/);
}

// Test linking of intrinsics
{
  let linkInstance = wasmEvalText(`(module
    (import "" "i8vecmul" (func (param i32 i32 i32 i32)))
  )`, {"": instance.exports});
}
