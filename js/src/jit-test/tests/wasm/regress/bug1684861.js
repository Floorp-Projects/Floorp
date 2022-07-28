const oob = /index out of bounds/;
const unaligned = /unaligned memory access/;
const RuntimeError = WebAssembly.RuntimeError;

// Test memory.atomic.notify unaligned access.
const module = new WebAssembly.Module(wasmTextToBinary(`(module
  (type (;0;) (func))
  (func $main (type 0)
    i32.const -64
    i32.const -63
    memory.atomic.notify offset=1
    unreachable)
  (memory (;0;) 4 4)
  (export "main" (func $main)))`));

const instance = new WebAssembly.Instance(module);
assertErrorMessage(() => instance.exports.main(), RuntimeError, unaligned);

// Test memory.atomic.notify oob access.
const module2 = new WebAssembly.Module(wasmTextToBinary(`(module
  (type (;0;) (func))
  (func $main (type 0)
    i32.const -64
    i32.const -63
    memory.atomic.notify offset=65536
    unreachable)
  (memory (;0;) 4 4)
  (export "main" (func $main)))`));

const instance2 = new WebAssembly.Instance(module2);
assertErrorMessage(() => instance2.exports.main(), RuntimeError, oob);

// Test memory.atomic.wait32 and .wait64 unaligned access.
const module3 = new WebAssembly.Module(wasmTextToBinary(`(module
  (type (;0;) (func))
  (func $wait32 (type 0)
    i32.const -64
    i32.const 42
    i64.const 0
    memory.atomic.wait32 offset=1
    unreachable)
  (func $wait64 (type 0)
    i32.const -64
    i64.const 43
    i64.const 0
    memory.atomic.wait64 offset=3
    unreachable)
  (memory (;0;) 4 4 shared)
  (export "wait32" (func $wait32))
  (export "wait64" (func $wait64)))`));

const instance3 = new WebAssembly.Instance(module3);
assertErrorMessage(() => instance3.exports.wait32(), RuntimeError, unaligned);
assertErrorMessage(() => instance3.exports.wait64(), RuntimeError, unaligned);
