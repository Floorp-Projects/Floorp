// Bug 1404760: Optimized compare-and-branch with a preserved value would fail
// the baseline compiler on x86 debug builds (and would just generate bad code
// on non-debug builds) because of register starvation.

wasmEvalText(
    `(module
      (func $run (param i64) (param i64) (result i64)
        block i64
	  i64.const 1
          (i64.lt_s (local.get 0) (local.get 1))
	  br_if 0
	  drop
          i64.const 2
        end)
      (export "run" $run))`
);
