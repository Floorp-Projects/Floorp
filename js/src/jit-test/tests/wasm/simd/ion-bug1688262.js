// |jit-test| skip-if: !wasmSimdEnabled()

// Shuffle pattern incorrectly recognized as a rotate due to a missing guard in
// the optimizer.

let ins = wasmEvalText(`
  (module
    (memory (export "mem") 1)
    (func (export "test")
      (v128.store (i32.const 0)
        (i8x16.shuffle 0 1 2 3 4 5 6 7 8 0 1 2 3 4 5 6
                       (v128.load (i32.const 16))
                       (v128.const i32x4 0 0 0 0)))))
`);

let mem = new Int8Array(ins.exports.mem.buffer);
let input = [10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25];
let output = [10, 11, 12, 13, 14, 15, 16, 17, 18, 10, 11, 12, 13, 14, 15, 16];
mem.set(input, 16);
ins.exports.test();
let result = Array.from(mem.subarray(0, 16));
assertDeepEq(output, result);
