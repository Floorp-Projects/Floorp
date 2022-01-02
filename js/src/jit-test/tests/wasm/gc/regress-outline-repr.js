// |jit-test| skip-if: !wasmGcEnabled()

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
      (field (mut eqref))))
  (type $S2 (struct))

  (func $main
    (struct.set $S 18
      (struct.new_with_rtt $S
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
        (ref.null eq)
        (rtt.canon $S))
      (struct.new_with_rtt $S2 (rtt.canon $S2))))
  (start $main))
`
wasmEvalText(wat);

// Test subtyping across outline/inline representations works

wasmEvalText(`
(module
  (type $outline
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
      (field (mut i64))))
  (type $inline
    (struct
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
      (field (mut i64))
    ))

  (func $main
    (local $outline (ref null $outline))
    (local $inline (ref null $inline))

    (; create an outline object and acquire multiple views to it ;)
    (struct.new_with_rtt $outline
          (i64.const 0xFF)
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
          (rtt.canon $outline))
    local.tee $outline
    local.set $inline

    (; clobber the object header ;)
    (struct.set $inline 0
      local.get $inline
      i64.const 0
    )
    (struct.set $inline 1
      local.get $inline
      i64.const 0
    )
    (struct.set $inline 2
      local.get $inline
      i64.const 0
    )
    (struct.set $inline 3
      local.get $inline
      i64.const 0
    )
    (struct.set $inline 4
      local.get $inline
      i64.const 0
    )

    (; try to read a field ;)
    (struct.get $outline 0
      local.get $outline
    )
    drop
  )
  (start $main))
`);
