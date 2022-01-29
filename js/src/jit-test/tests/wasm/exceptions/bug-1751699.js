// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
	wasmEvalText(`
      (import "" "" (func $d))
      (func try call $d end)
    `);
});
