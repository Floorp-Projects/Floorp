// |jit-test| allow-oom; slow; skip-if: !canRunHugeMemoryTests()

// This is just like memory-fill.js, but it is marked slow because the
// C++-UB-safe operations we use for out-of-line fills on shared memory are slow
// and the >4GB fill takes a long time.

try {
    var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (memory (export "mem") i64 65537 65537 shared)
  (func (export "f") (param $p i64) (param $c i32) (param $n i64)
    (memory.fill (local.get $p) (local.get $c) (local.get $n))))`)));
} catch (e) {
    if (e instanceof WebAssembly.RuntimeError && String(e).match(/too many memory pages/)) {
        quit(0);
    }
    throw e;
}

var mem = new Uint8Array(ins.exports.mem.buffer);

// Fill above 4GB
doit(mem, 0x100000100, 37, 14);

// Fill OOB above 4GB
assertErrorMessage(() => ins.exports.f(0x10000FFFFn, 66, 14n),
                   WebAssembly.RuntimeError,
                   /out of bounds/);
assertEq(mem[0x10000FFFF], 0);
assertEq(mem[mem.length-1], 0);

// Fill across 4GB
doit(mem, 0x100000000 - 16, 42, 32);

// Fill more than 4GB...
ins.exports.f(0n, 86, 65536n*65537n);
assertEq(mem[mem.length-1], 86);
assertEq(mem[0], 86);

// Fill OOB more than 4GB
assertErrorMessage(() => ins.exports.f(1n, 75, 65536n*65537n),
                   WebAssembly.RuntimeError,
                   /out of bounds/);
assertEq(mem[1], 86);
assertEq(mem[mem.length-1], 86);

function doit(mem, addr, c, n) {
    assertEq(mem[addr-1], 0);
    assertEq(mem[addr], 0);
    assertEq(mem[addr + n - 1], 0);
    assertEq(mem[addr + n], 0);
    ins.exports.f(BigInt(addr), c, BigInt(n));
    assertEq(mem[addr-1], 0);
    assertEq(mem[addr], c);
    assertEq(mem[addr + n - 1], c);
    assertEq(mem[addr + n], 0);
}

