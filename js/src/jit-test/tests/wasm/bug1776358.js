let func = wasmEvalText(`(module
    (func
        (param f64) (param f64) (param f64) (param f64) (param f64)
        (param f64) (param f64) (param f64) (param f64)
    )
    (func
        (param i32)
        (result i32)
        ;; Call in a dead branch.
        (if (i32.const 0) (then
          (call 0
            (f64.const 0) (f64.const 0) (f64.const 0) (f64.const 0) (f64.const 0)
            (f64.const 0) (f64.const 0) (f64.const 0) (f64.const 0)
          )
        ))
        ;; Division to trigger a trap.
        (i32.div_s (get_local 0) (get_local 0))
    )
    (export "" (func 1))
)`).exports[""];
assertEq(func(7), 1);
let ex;
try {
    func(0);
} catch (e) {
    ex = e;
}
assertEq(ex instanceof WebAssembly.RuntimeError, true);
