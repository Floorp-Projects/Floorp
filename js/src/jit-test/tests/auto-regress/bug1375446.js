// |jit-test| allow-oom

loadFile(`
    disassemble(function() {
	return assertDeepEq(x.concat(obj), [1, 2, 3, "hey"]);
    })
`);
function loadFile(lfVarx) {
    oomTest(new Function(lfVarx));
}
