// |jit-test| allow-oom; skip-if: !canRunHugeMemoryTests()

const maxPages = wasmMaxMemoryPages("i64");
const m = new WebAssembly.Memory({initial: 0, index: "i64"});
try {
    m.grow(maxPages);
    assertEq(m.buffer.byteLength, maxPages * PageSizeInBytes);
} catch (e) {
    assertEq(e.message.includes("failed to grow"), true); // OOM
}
