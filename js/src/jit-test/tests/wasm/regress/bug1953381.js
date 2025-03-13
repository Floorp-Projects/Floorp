// |jit-test| skip-if: !wasmSimdEnabled()

for (let [mask, exp] of [
    [0xFF000000, 0x12bcdef0], // optimized to PBLENDVB on x86
    [0x00FFFFFF, 0x9a345678], // optimized to PBLENDVB on x86
    [0xFFFF0000, 0x1234def0], // optimized to PBLENDW on x86
    [0x0000FFFF, 0x9abc5678], // optimized to PBLENDW on x86
    [0x10000000, 0x9abcdef0], // non-optimizable mask
    [0x9FFFFFFF, 0x12345678], // non-optimizable mask
    [0x00000000, 0x9abcdef0],
    [0xFFFFFFFF, 0x12345678],
]) {
    const ins = wasmEvalText(`(module
        (memory (export "memory") 1 1)
        (func (export "test")
          i32.const 48
          i32.const 0
          v128.load
          i32.const 16
          v128.load
          v128.const i32x4 ${mask} ${mask} ${mask} ${mask}
          v128.bitselect
          v128.store
        )
        (data (i32.const 0)  "\\78\\56\\34\\12\\78\\56\\34\\12\\78\\56\\34\\12\\78\\56\\34\\12")
        (data (i32.const 16) "\\f0\\de\\bc\\9a\\f0\\de\\bc\\9a\\f0\\de\\bc\\9a\\f0\\de\\bc\\9a")
    )`);
    ins.exports.test();
    var result = new DataView(ins.exports.memory.buffer).getUint32(48, true);
    var expected = exp >>> 0;
    assertEq(result, expected);
}
