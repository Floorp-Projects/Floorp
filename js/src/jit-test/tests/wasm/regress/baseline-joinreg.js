load(libdir + "wasm.js");

// Bug 1322288 is about the floating join register not being reserved properly
// in the presence of boolean evaluation for control.  The situation is that a
// conditional branch passes a floating point value to the join point; the join register
// must be available when it does that, but could have been allocated to the operands of
// the conditional expression of the control flow.
//
// This test is white-box: it depends on the floating join reg being among the first
// floating registers to be allocated.

wasmEvalText(`
(module
 (func $run
  (drop (block f64
   (drop (br_if 0 (f64.const 1) (f64.eq (f64.const 1) (f64.const 0))))
   (drop (br 0 (f64.const 2))))))
 (export "run" $run))`);
