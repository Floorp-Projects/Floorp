if (!('oomTest' in this))
    quit();

lfLogBuffer = `this[''] = function() {}`;
loadFile(lfLogBuffer);
loadFile(lfLogBuffer);
function loadFile(lfVarx) {
    return oomTest(function() { return parseModule(lfVarx); });
}
