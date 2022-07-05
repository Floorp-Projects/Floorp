// |jit-test| skip-if: !('oomTest' in this)

loadFile(`
function parseAndEvaluate(source) {
    let m = parseModule(source);
    moduleLink(m);
}
parseAndEvaluate("async function a() { await 2 + 3; }")
`);
function loadFile(lfVarx) {
    oomTest(function() {
        eval(lfVarx);
    });
}
