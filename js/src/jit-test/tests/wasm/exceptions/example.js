// A diagram depicting the control flow graph of the function g below can be
// found in "js/src/wasm/WasmIonCompile.cpp". If you make any changes to this
// test file be sure to adjust the SMDOC documentation there as well.

let g = wasmEvalText(
  `(module
     (tag $exn (param f64))
     (func $f)
     (func (export "g") (param $arg i32) (result f64)
       (local.get $arg)
       (try (param i32) (result f64)
         (do
           (if (result f64)
             (then
               (f64.const 3))
             (else
               (throw $exn (f64.const 6))))
           (call $f)
           (f64.sub (f64.const 2)))  ;; If $arg is 0 we end here, subtracting 3.
         ;; If $arg is not 0 then the else-block throws $exn, caught below.
         (catch $exn
           (f64.add (f64.const 4)))  ;; Adds 4 to the value in the $exn (6).
         (catch_all                  ;; This shouldn't occur.
           (f64.const 5)))))`
).exports.g;

assertEq(g(0), 10);
assertEq(g(1), 1);
