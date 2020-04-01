// |jit-test| skip-if: !wasmReftypesEnabled()

let counter;
function resetCounter() { counter = 0; }

function boxNextInt() { return {val: counter++}; }
function unboxInt(box) { return box.val; }
function boxNextThreeInts() {
    return [boxNextInt(), boxNextInt(), boxNextInt()];
}
function unboxThreeInts(x, y, z) {
    return [unboxInt(x), unboxInt(y), unboxInt(z)];
}

function testAddNextThreeInts(text, imports) {
    let i = new WebAssembly.Instance(
        new WebAssembly.Module(wasmTextToBinary(text)), { imports });

    resetCounter();
    for (let n = 0; n < 100000; n += 3) {
        assertEq(i.exports.addNextThreeInts(), n * 3 + 3);
    }
}

testAddNextThreeInts(`
      (module
        (func $boxNextInt (import "imports" "boxNextInt")
          (result anyref))
        (func $unboxInt (import "imports" "unboxInt")
          (param anyref) (result i32))

        (func $boxNextThreeInts (result anyref anyref anyref)
          call $boxNextInt
          call $boxNextInt
          call $boxNextInt)

        (func $unboxThreeInts (param anyref anyref anyref) (result i32 i32 i32)
          local.get 0
          call $unboxInt
          local.get 1
          call $unboxInt
          local.get 2
          call $unboxInt)

        (func $addNextThreeInts (export "addNextThreeInts") (result i32)
          call $boxNextThreeInts
          call $unboxThreeInts
          i32.add
          i32.add))`,
                   {boxNextInt, unboxInt});

testAddNextThreeInts(`
      (module
        (func $boxNextThreeInts (import "imports" "boxNextThreeInts")
          (result anyref anyref anyref))
        (func $unboxThreeInts (import "imports" "unboxThreeInts")
          (param anyref anyref anyref) (result i32 i32 i32))

        (func $addNextThreeInts (export "addNextThreeInts") (result i32)
          call $boxNextThreeInts
          call $unboxThreeInts
          i32.add
          i32.add))`,
                   {boxNextThreeInts, unboxThreeInts});
