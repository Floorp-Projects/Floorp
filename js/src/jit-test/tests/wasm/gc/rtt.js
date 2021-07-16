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
