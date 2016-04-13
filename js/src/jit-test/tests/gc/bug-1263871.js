if (!('oomTest' in this))
    quit();

lfLogBuffer = `this[''] = function() {}`;
loadFile(lfLogBuffer);
loadFile(lfLogBuffer);
function loadFile(lfVarx)
    oomTest(function() parseModule(lfVarx))
