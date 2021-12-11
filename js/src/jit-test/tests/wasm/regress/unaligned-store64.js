var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (memory (export "mem") 1 1)
       (func (export "store_32_1") (param $ptr i32)
         (i64.store32 align=1 (local.get $ptr) (i64.const 0xabba1337)))
       (func (export "store_32_2") (param $ptr i32)
         (i64.store32 align=2 (local.get $ptr) (i64.const 0xabba1337)))
       (func (export "store_16") (param $ptr i32)
         (i64.store16 align=1 (local.get $ptr) (i64.const 0x1337))))`))).exports;

var mem = new Uint8Array(ins.mem.buffer);

ins.store_16(1);
assertEq(mem[1], 0x37);
assertEq(mem[2], 0x13);

ins.store_32_1(11);
assertEq(mem[11], 0x37);
assertEq(mem[12], 0x13);
assertEq(mem[13], 0xba);
assertEq(mem[14], 0xab);

ins.store_32_2(18);
assertEq(mem[18], 0x37);
assertEq(mem[19], 0x13);
assertEq(mem[20], 0xba);
assertEq(mem[21], 0xab);

// This must also work on all platforms even though we're lying about the
// alignment.
ins.store_32_2(29);
assertEq(mem[29], 0x37);
assertEq(mem[30], 0x13);
assertEq(mem[31], 0xba);
assertEq(mem[32], 0xab);
