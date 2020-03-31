// |jit-test| skip-if: !wasmReftypesEnabled()

let counter = 0;
let i = new WebAssembly.Instance(
    new WebAssembly.Module(wasmTextToBinary(`
      (module
        (func $boxNextInt (import "imports" "boxNextInt")
          (result anyref))
        (func $unboxInt (import "imports" "unboxInt")
          (param anyref) (result i32))

        (func $boxNextTwoInts (result anyref anyref)
          call $boxNextInt
          call $boxNextInt)

        (func $unboxTwoInts (param anyref anyref) (result i32 i32)
          local.get 0
          call $unboxInt
          local.get 1
          call $unboxInt)

        (func $addNextTwoInts (export "addNextTwoInts") (result i32)
          call $boxNextTwoInts
          call $unboxTwoInts
          i32.add))`)),
    {imports: {boxNextInt: () => ({val: counter++}),
               unboxInt: box => box.val}});

for (let n = 0; n < 200000; n += 2) {
    assertEq(i.exports.addNextTwoInts(), n * 2 + 1);
}
