

function test1() {
    test();
}

function test() {
    var du = new Debugger();
    du.setupTraceLoggerScriptCalls();
    du.startTraceLogger();
}

var du = new Debugger();
if (typeof du.setupTraceLoggerScriptCalls == "function")
    test1();
