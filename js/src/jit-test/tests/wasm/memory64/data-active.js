// |jit-test| allow-oom; skip-if: !canRunHugeMemoryTests()

function testIt(s, addr) {
    try {
        var ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (memory (export "mem") i64 65537)
  (data (i64.const ${addr}) "${s}"))`)));
    } catch (e) {
        if (e instanceof WebAssembly.RuntimeError && String(e).match(/too many memory pages/)) {
            return;
        }
        throw e;
    }
    var mem = new Uint8Array(ins.exports.mem.buffer);
    assertEq(String.fromCodePoint(...mem.slice(addr, addr + s.length)), s)
}

// Test that an active data segment targeting the area above 4GB works as expected
testIt("hello, world!", 0x100000020);

// Ditto, but crosses the 4GB boundary
var s = "supercalifragilisticexpialidocious";
testIt(s, 0x100000000 - Math.floor(s.length / 2));

// This is OOB above the 4GB boundary - throws OOB during instantiation.
// But can also throw for OOM when trying to create the memory.
assertErrorMessage(() => new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
  (memory (export "mem") i64 65537)
  (data (i64.const ${65536*65537-10}) "supercalifragilisticexpialidocious"))`))),
                   WebAssembly.RuntimeError,
                   /(out of bounds)|(too many memory pages)/);

