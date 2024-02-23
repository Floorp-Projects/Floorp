oomTest(() => {
    const module = wasmBuiltinI8VecMul();
    WebAssembly.Module.imports(module);
});
