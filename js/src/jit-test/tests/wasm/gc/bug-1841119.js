// |jit-test| --fuzzing-safe; --ion-offthread-compile=off; skip-if: !wasmGcEnabled()

function f() {
}

gczeal(9,10);

let t = wasmEvalText(`
(module
  (type (struct))
  (table (export "table") (ref null 0)
    (elem ( ref.null 0 ))
  )
  (global (export "global") (ref null 0) ref.null 0)
  (tag (export "tag") (param (ref null 0)))
)`).exports;

f();
