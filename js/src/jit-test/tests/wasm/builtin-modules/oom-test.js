// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
    const module = wasmBuiltinI8VecMul();
    WebAssembly.Module.imports(module);
});
