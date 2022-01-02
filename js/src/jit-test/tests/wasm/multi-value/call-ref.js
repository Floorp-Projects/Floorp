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

function testAddNextThreeIntsInner(addNextThreeInts) {
    resetCounter();
    for (let n = 0; n < 100000; n += 3) {
        assertEq(addNextThreeInts(), n * 3 + 3);
    }
}

function testAddNextThreeInts(text, imports) {
    let i = new WebAssembly.Instance(
        new WebAssembly.Module(wasmTextToBinary(text)), { imports });

    testAddNextThreeIntsInner(() => i.exports.addNextThreeInts());
}

testAddNextThreeInts(`
      (module
        (func $boxNextInt (import "imports" "boxNextInt")
          (result externref))
        (func $unboxInt (import "imports" "unboxInt")
          (param externref) (result i32))

        (func $boxNextThreeInts (result externref externref externref)
          call $boxNextInt
          call $boxNextInt
          call $boxNextInt)

        (func $unboxThreeInts (param externref externref externref) (result i32 i32 i32)
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
          (result externref externref externref))
        (func $unboxThreeInts (import "imports" "unboxThreeInts")
          (param externref externref externref) (result i32 i32 i32))

        (func $addNextThreeInts (export "addNextThreeInts") (result i32)
          call $boxNextThreeInts
          call $unboxThreeInts
          i32.add
          i32.add))`,
                   {boxNextThreeInts, unboxThreeInts});

{
    let i = wasmEvalText(`
      (module
        (func $boxNextThreeInts (import "imports" "boxNextThreeInts")
          (result externref externref externref))

        (func (export "boxNextThreeInts") (result externref externref externref)
          call $boxNextThreeInts))`,
                         {imports: {boxNextThreeInts}});
    testAddNextThreeIntsInner(() => {
        let [a, b, c] = i.exports.boxNextThreeInts();
        return unboxInt(a) + unboxInt(b) + unboxInt(c);
    });
}
