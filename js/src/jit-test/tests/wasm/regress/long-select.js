load(libdir + "wasm.js");

// Bug 1337060 causes too much register pressure on x86 by requiring four int64
// values in registers at the same time.

setJitCompilerOption('wasm.test-mode', 1);

wasmFullPassI64(`
(module
  (func (result i64)
    i64.const 0x2800000033
    i64.const 0x9900000044
    i64.const 0x1000000012
    i64.const 0x1000000013
    i64.lt_s
    select) (export "run" 0))`, createI64("0x2800000033"));

wasmFullPassI64(`
(module
  (func (result i64)
    i64.const 0x2800000033
    i64.const 0x9900000044
    i64.const 0x1000000013
    i64.const 0x1000000012
    i64.lt_s
    select) (export "run" 0))`, createI64("0x9900000044"));
