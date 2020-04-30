// |jit-test| skip-if: !wasmReftypesEnabled() || !wasmGcEnabled() || wasmCompileMode() != 'baseline'

// White-box test for bug 1617908.  The significance of this test is that the
// type $S is too large to fit in an inline TypedObject, and the write barrier
// logic must take this into account when storing the (ref $S2) into the last
// field of the object.

const wat = `
(module
  (type $S
    (struct
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut anyref))))
  (type $S2 (struct))

  (func $main
    (struct.set $S 18
      (struct.new $S
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (i64.const 0)
        (ref.null))
      (struct.new $S2)))
  (start $main))
`
wasmEvalText(wat);
