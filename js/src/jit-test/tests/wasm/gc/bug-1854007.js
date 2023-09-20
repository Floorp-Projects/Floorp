// |jit-test| test-also=--wasm-test-serialization; skip-if: !wasmGcEnabled()

let {run} = wasmEvalText(`(module
  (rec (type $$t1 (func (result (ref null $$t1)))))
  (rec (type $$t2 (func (result (ref null $$t2)))))

  (func $$f1 (type $$t1) (ref.null $$t1))
  (func $$f2 (type $$t2) (ref.null $$t2))
  (table funcref (elem $$f1 $$f2))

  (func (export "run")
    (call_indirect (type $$t2) (i32.const 0))
    drop
  )
)`).exports;
run();
