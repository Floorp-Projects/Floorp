// |jit-test| skip-if: !wasmSimdEnabled()

// Check if GVN indentifies two non-indentical shuffles. During value numbering
// the control field/data might look the same. Shuffle or permute kind, and
// operands order have to be taking into account during value numbering.
// If GVN fails to recognize the following shuffles as different, the v128.xor
// produces zero output.
var ins = wasmEvalText(`(module
    (memory (export "memory") 1 1)
    (func $test (param v128) (result v128)
        local.get 0
        v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000
        i8x16.shuffle 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
        v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000
        local.get 0
        i8x16.shuffle 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
        v128.xor
    )
    (func (export "run")
        i32.const 16
        i32.const 0
        v128.load
        call $test
        v128.store
    )
)`);

const mem64 = new BigInt64Array(ins.exports.memory.buffer, 0, 4);
mem64[0] = 0x123456789n;
mem64[1] = -0xFDCBA000n;
ins.exports.run();
assertEq(mem64[2], -0xFDCBA000n);
assertEq(mem64[3], 0x123456789n);
