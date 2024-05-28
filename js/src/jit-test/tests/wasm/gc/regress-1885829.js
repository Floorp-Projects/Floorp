// |jit-test| slow; skip-if: !wasmGcEnabled()

const { test } = wasmEvalText(`(module
  (type $t1 (array (mut i16)))
  (type $t3 (sub (array (mut (ref $t1)))))
  (func (export "test")
    i32.const 0
    i32.const 20
    array.new $t1
    i32.const 3
    array.new $t3
    i32.const 1
    i32.const 2
    i32.const 7
    array.new $t1
    array.set $t3

    call 0
  )
)`).exports;
assertErrorMessage(() => test(), InternalError, /too much recursion/);
