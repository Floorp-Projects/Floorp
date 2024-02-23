oomTest(() => {
	wasmEvalText(`
      (import "" "" (func $d))
      (func try call $d end)
    `);
});
