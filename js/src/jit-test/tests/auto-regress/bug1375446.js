// |jit-test| allow-oom; skip-if: !('oomTest' in this)

loadFile(`
    disassemble(function() {
	return assertDeepEq(x.concat(obj), [1, 2, 3, "hey"]);
    })
`);
function loadFile(lfVarx) {
    oomTest(new Function(lfVarx));
}
