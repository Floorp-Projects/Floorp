oomTest(() => {
    wasmEvalText(`
      (tag $d)
      (func $anotherLocalFuncThrowsExn)
      (func throw $d)
      (func (try (do
        call $anotherLocalFuncThrowsExn
      )))
    `);
});
