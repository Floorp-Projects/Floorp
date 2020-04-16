let instance = wasmEvalText(`
  (func $twoRefs (result anyref anyref)
    (ref.null)
    (ref.null))
  (func $fourRefs (export "run") (result anyref anyref anyref anyref anyref anyref)
    call $twoRefs
    call $twoRefs
    call $twoRefs)
`);

assertDeepEq(instance.exports.run(), [null, null, null, null, null, null])
