if (!wasmGcEnabled()) {
    quit(0);
}

enableShellAllocationMetadataBuilder();
gczeal(9, 1);
new WebAssembly.Global({ value: 'i32' }, 42);
