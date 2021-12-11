// |jit-test| skip-if: !('oomTest' in this)
// Ensure that trap reporting mechanism doesn't crash under OOM conditions.

oomTest(
  wasmEvalText(
    `(module
       (func (export "f") (result)
         unreachable))`
  ).exports.f
);
