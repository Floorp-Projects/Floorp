// |jit-test| allow-oom; skip-if: !canRunHugeMemoryTests()

// Also see memory-copy-shared.js

var HEAPMIN=65538;
var HEAPMAX=65540;
var PAGESIZE=65536;

try {
    var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (memory (export "mem") i64 ${HEAPMIN} ${HEAPMAX})
  (func (export "f") (param $dst i64) (param $src i64) (param $n i64)
    (memory.copy (local.get $dst) (local.get $src) (local.get $n))))`)));
} catch (e) {
    if (e instanceof WebAssembly.RuntimeError && String(e).match(/too many memory pages/)) {
        quit(0);
    }
    throw e;
}

var mem = new Uint8Array(ins.exports.mem.buffer);
var lowbase = 0xffff_1000;
var highbase = 0x1_0000_8000;
for ( let n=0; n < 256; n++ ) {
    mem[n + lowbase] = n + 1;
    mem[n + highbase] = n + 1;
}

// Copy from above 4GB to below
doit(0, highbase, 60);

// Copy from above 4GB with OOB to below
assertErrorMessage(() => ins.exports.f(0n, BigInt(HEAPMIN*PAGESIZE-32768), 65536n),
                   WebAssembly.RuntimeError,
                   /out of bounds/);

// Copy from below 4GB to above
doit(0x1_0000_0100, lowbase, 60);

// Copy from below 4GB to above with OOB
assertErrorMessage(() => ins.exports.f(BigInt(HEAPMIN*PAGESIZE-32768), BigInt(lowbase), 65536n),
                   WebAssembly.RuntimeError,
                   /out of bounds/);

// Copy across the 4GB boundary
doit(0xffff_ff80, lowbase, 256);

// Copy more than 4GB.  Note overlap prevents full correctness checking.
ins.exports.f(BigInt(PAGESIZE), 0n, BigInt(PAGESIZE*(HEAPMIN-1)));
for ( let i=0 ; i < PAGESIZE; i++ )
    assertEq(mem[i + PAGESIZE], mem[i]);

function doit(dst, src, n) {
    ins.exports.f(BigInt(dst), BigInt(src), BigInt(n));
    for ( let i=0 ; i < n; i++ )
        assertEq(mem[dst + i], mem[src + i]);
}
