// |jit-test| skip-if: !('oomTest' in this)

var lfLogBuffer = `
evalInWorker(\`
        try { oomAfterAllocations(2); } catch(e) {}
    \`);
`;
loadFile("");
loadFile(lfLogBuffer);
function loadFile(lfVarx) {
    oomTest(function() {
        let m = parseModule(lfVarx);
        moduleLink(m);
        moduleEvaluate(m);
    });
}
