// Ensure that trap reporting mechanism doesn't crash under OOM conditions.

oomTest(
  wasmEvalText(
    `(module
       (func (export "f") (result)
         unreachable))`
  ).exports.f
);
