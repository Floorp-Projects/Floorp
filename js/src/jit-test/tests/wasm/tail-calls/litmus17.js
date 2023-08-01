// |jit-test| skip-if: !wasmTailCallsEnabled()

// Test that unwinding will find the right instance in the middle of a tail call
// chain.  This test is mostly useful on the simulators, as they will walk the
// stack, looking for the innermost instance, on every memory reference.  The
// logic of this test is that the setup intra-module tail call in mod2 will need
// to be recorded as possibly-inter-module because the subsequent inter-module
// call replaces the activation with one that is actually inter-module.

let ins1 = wasmEvalText(`
(module
  (memory (export "mem") 1 1)
  (func (export "memref") (result i32)
    (i32.load (i32.const 0))))`);

let ins2 = wasmEvalText(`
(module
  (import "mod1" "memref" (func $memref (result i32)))

  (func (export "run") (result i32)
    (return_call $g))

  (func $g (result i32)
    (return_call $memref)))`, {mod1: ins1.exports});

(new Int32Array(ins1.exports.mem.buffer))[0] = 1337;
assertEq(ins2.exports.run(), 1337);
