// |jit-test| skip-if: !('oomTest' in this)

loadFile("");
loadFile("");
loadFile(` function lalala() {}
    new Map([[1, 2]]).forEach(lalala)
    `);
function loadFile(lfVarx) {
    return oomTest(function() {
        eval(lfVarx)
    });
}
