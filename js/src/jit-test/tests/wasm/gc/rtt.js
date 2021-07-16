// |jit-test| skip-if: !wasmGcEnabled()

// Multiple invocations of rtt.sub are cached
{
  let {createSubA, isSubA} = wasmEvalText(`(module
    (type $a (struct))
    (func (export "createSubA") (result eqref)
      rtt.canon $a
      rtt.sub $a
      struct.new_with_rtt $a
    )
    (func (export "isSubA") (param eqref) (result i32)
      local.get 0
      rtt.canon $a
      rtt.sub $a
      ref.test
    )
  )`).exports;

  assertEq(isSubA(createSubA()), 1);
}

// rtt.canon is a constant expression
{
  let {createA, isA} = wasmEvalText(`(module
    (; an immediate function type to cause renumbering ;)
    (type (func))
    (type $a (struct))
    (global $aCanon (rtt $a) rtt.canon $a)
    (func (export "createA") (result eqref)
      global.get $aCanon
      struct.new_with_rtt $a
    )
    (func (export "isA") (param eqref) (result i32)
      local.get 0
      global.get $aCanon
      ref.test
    )
  )`).exports;

  assertEq(isA(createA()), 1);
}

// rtt.sub is a constant expression
{
  let {createSubA, isSubA} = wasmEvalText(`(module
    (; an immediate function type to cause renumbering ;)
    (type (func))
    (type $a (struct))
    (global $aSub (rtt $a) rtt.canon $a rtt.sub $a)
    (func (export "createSubA") (result eqref)
      global.get $aSub
      struct.new_with_rtt $a
    )
    (func (export "isSubA") (param eqref) (result i32)
      local.get 0
      global.get $aSub
      ref.test
    )
  )`).exports;

  assertEq(isSubA(createSubA()), 1);
}
