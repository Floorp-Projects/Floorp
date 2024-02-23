// |jit-test| allow-oom
try {
    for (let i = 0; i < 5; i++) {
        WebAssembly.instantiateStreaming(
            wasmTextToBinary('(module (func) (export "" (func 0)))')
        );
    }
} catch (e) {}
oomAtAllocation(7, 7);
