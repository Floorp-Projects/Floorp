// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
	wasmEvalText(`(import "" "" (tag $undef)) (func throw 0) (func (try (do)))`);
});
