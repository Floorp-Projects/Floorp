// |jit-test| skip-if: !('oomTest' in this)

lfLogBuffer = `this[''] = function() {}`;
loadFile(lfLogBuffer);
loadFile(lfLogBuffer);
function loadFile(lfVarx) {
    return oomTest(function() { return parseModule(lfVarx); });
}
