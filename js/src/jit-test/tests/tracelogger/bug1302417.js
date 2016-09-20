
if (!('oomTest' in this))
    quit();

if (typeof new Debugger().setupTraceLoggerScriptCalls == "function") {
    lfLogBuffer = `
        var du = new Debugger;
        du.setupTraceLoggerScriptCalls();
        startTraceLogger();
    `;
    loadFile(lfLogBuffer);
    function loadFile(lfVarx) {
        oomTest(function() {
                m = parseModule(lfVarx);
                m.declarationInstantiation();
                m.evaluation();
        })
    }
}

