if (!('oomTest' in this))
    quit();

loadFile("");
loadFile("");
loadFile(` function lalala() {}
    new Map([[1, 2]]).forEach(lalala)
    `);
function loadFile(lfVarx) oomTest(function() {
    eval(lfVarx)
})
