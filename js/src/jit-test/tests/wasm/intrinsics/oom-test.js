// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
    const module = wasmIntrinsicI8VecMul();
    WebAssembly.Module.imports(module);
});
