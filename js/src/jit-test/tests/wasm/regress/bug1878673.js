// Check proper handling of OOM in SIMD loads.

oomTest(function () {
    let x = wasmTextToBinary(`(module
        (memory 1 1)
        (func
          i32.const 16
          v128.load8x8_s
          i16x8.abs
          drop)
        )`);
    new WebAssembly.Module(x);
});
