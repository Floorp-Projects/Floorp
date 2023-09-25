// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || (!getBuildConfiguration("x86") && !getBuildConfiguration("x64")) || getBuildConfiguration("simulator")

const avx = isAvxPresent();
for (let [n1, n2, numInstr] of [
    [0x123456789abn, 0xffeeddccbbaa9988n, avx ? 6 : 7],
    [42n, 0xFFFFFFFFn, avx ? 3 : 4],
    [0n, 0n, 1],
    [1n, 1n, 1],
    ...iota(63).map(i => [2n << BigInt(i), 2n << BigInt(i), 1]),
    ...iota(63).reduce((acc, i) => {
        const base = 2n << BigInt(i);
        return acc.concat(iota(i + 1).map(j => [
            base | (1n << BigInt(j)), base | (1n << BigInt(j)),
            (avx ? 2 : 3) + (j == 0 ? 0 : 1)]));
    }, []),
    ...iota(63).map(i => [~(1n << BigInt(i)), ~(1n << BigInt(i)), avx ? 5 : 6]),
    [0x7fffffffffffffffn, 0x7fffffffffffffffn, avx ? 5 : 6],
    [-1n, -1n, 3],
]) {
    var wasm = wasmTextToBinary(`(module
        (memory (export "memory") 1 1)
        (func $t (export "t") (param v128) (result v128)
            local.get 0
            v128.const i64x2 ${n1} ${n2}
            i64x2.mul
        )
        (func $t0 (param v128 v128) (result v128)
            local.get 0
            local.get 1
            i64x2.mul
        )
        (func (export "run") (result i32)
            i32.const 16
            i32.const 0
            v128.load
            call $t
            v128.store

            v128.const i64x2 ${n1} ${n2}
            i32.const 0
            v128.load
            call $t0

            i32.const 16
            v128.load
            i64x2.eq
            i64x2.all_true
        )
    )`)

    var ins = new WebAssembly.Instance(new WebAssembly.Module(wasm));
    var mem64 = new BigInt64Array(ins.exports.memory.buffer, 0, 4);

    for (let [t1, t2] of [
        [1n, 1n], [-1n, -2n], [0n, 0n], [0x123456789abn, 0xffeeddccbbaa9988n],
        [0x100001111n, -0xFF0011n], [5555n, 0n],
    ]) {
        mem64[0] = t1; mem64[1] = t2;
        assertEq(ins.exports.run(), 1);
    }

    if (hasDisassembler() && getBuildConfiguration("x64")) {
        const dis = wasmDis(ins.exports.t, {asString: true,});
        const lines = getFuncBody(dis).trim().split('\n');
        assertEq(lines.length, numInstr);
    }
}

// Utils.
function getFuncBody(dis) {
    const parts = dis.split(/mov %rsp, %rbp\n|^[0-9A-Fa-f ]+pop %rbp/gm);
    return parts.at(-2).replace(/[0-9A-F]{8} (?: [0-9a-f]{2})+[\s\n]+/g, "");
}
