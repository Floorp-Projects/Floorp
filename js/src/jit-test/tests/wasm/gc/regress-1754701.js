// |jit-test| skip-if: !wasmGcEnabled() || !('oomTest' in this)

let { testArray, testStructInline, testStructOutline } = wasmEvalText(`
  (module
  (type $array (array i32))
  (type $structInline (struct))
  (type $structOutline
    (struct
      ${`(field i32)`.repeat(100)}
    )
  )
  (func (export "testArray")
    (param i32)
    (result eqref)
    local.get 0
    array.new_default $array
  )
  (func (export "testStructInline")
    (result eqref)
    struct.new_default $structInline
  )
  (func (export "testStructOutline")
    (result eqref)
    struct.new_default $structOutline
  )
 )
`).exports
oomTest(() => testArray(1));
oomTest(() => testStructInline());
oomTest(() => testStructOutline());
