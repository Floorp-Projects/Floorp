// |jit-test| allow-oom; skip-if: !canRunHugeMemoryTests()

var S = (function () {
    let s = "";
    for ( let i=0; i < 16; i++ )
        s += "0123456789abcdef"
    return s;
})();

for (let shared of ['', 'shared']) {
    try {
        var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (memory (export "mem") i64 65537 65537 ${shared})
  (data $d "${S}")
  (func (export "f") (param $p i64) (param $o i32) (param $n i32)
    (memory.init $d (local.get $p) (local.get $o) (local.get $n))))`)));
    } catch (e) {
        if (e instanceof WebAssembly.RuntimeError && String(e).match(/too many memory pages/)) {
            quit(0);
        }
        throw e;
    }

    var mem = new Uint8Array(ins.exports.mem.buffer);

    // Init above 4GB
    doit(mem, 0x1_0000_1000, 1, S.length-1);

    // Init above 4GB with OOM
    assertErrorMessage(() => ins.exports.f(0x1_0000_ff80n, 0, 256),
                       WebAssembly.RuntimeError,
                       /out of bounds/);

    // Init across 4GB
    doit(mem, 0xffff_ff80, 3, 200);
}

function doit(mem, addr, offs, n) {
    ins.exports.f(BigInt(addr), offs, n);
    for (let i=0; i < n; i++) {
        assertEq(mem[addr+i], S.charCodeAt(offs+i));
    }
}

