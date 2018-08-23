if (!('oomTest' in this))
    quit();

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
        instantiateModule(m);
        evaluateModule(m);
    });
}
