// |jit-test| skip-if: !wasmReftypesEnabled()

// |jit-test| skip-if: !wasmReftypesEnabled()

let instance = wasmEvalText(`
  (func $twoRefs (result anyref anyref)
    (ref.null extern)
    (ref.null extern))
  (func $fourRefs (export "run") (result anyref anyref anyref anyref anyref anyref)
    call $twoRefs
    call $twoRefs
    call $twoRefs)
`);

assertDeepEq(instance.exports.run(), [null, null, null, null, null, null])
