// |jit-test| allow-oom; skip-if: !canRunHugeMemoryTests()

// This tests that we can grow the heap by more than 4GB.  Other grow tests are
// in basic.js.

for (let shared of ['', 'shared']) {
    try {
        var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (memory (export "mem") i64 0 65540 ${shared})
  (func (export "f") (param $delta i64) (result i64)
    (memory.grow (local.get $delta))))`)));
    } catch (e) {
        if (e instanceof WebAssembly.RuntimeError && String(e).match(/too many memory pages/)) {
            quit(0);
        }
        throw e;
    }

    let res = ins.exports.f(65537n);
    if (res === -1n) {
        quit(0);                    // OOM
    }
    assertEq(ins.exports.mem.buffer.byteLength, 65537*65536);
    let mem = new Uint8Array(ins.exports.mem.buffer);
    mem[65537*65536-1] = 37;
    assertEq(mem[65537*65536-1], 37);

    assertEq(ins.exports.f(4n), -1n);
}

