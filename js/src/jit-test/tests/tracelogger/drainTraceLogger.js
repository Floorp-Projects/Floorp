function TestDrainTraceLoggerInvariants(obj) {
    var scripts = 0;
    var stops = 0;
    for (var i = 0; i < objs.length; i++) {
        if (objs[i].logType == "Script") {
            scripts++;
            assertEq("fileName" in objs[i], true);
            assertEq("lineNumber" in objs[i], true);
            assertEq("columnNumber" in objs[i], true);
        } else if (objs[i].logType == "Stop") {
            stops++;
        } else {
            assertEq(true, false);
        }
    }
    assertEq(scripts, stops + 1);
    // "+ 1" because we get a start for drainTraceLogger.js:1, but not the stop.
}

function GetMaxScriptDepth(obj) {
    var max_depth = 0;
    var depth = 0;
    for (var i = 0; i < objs.length; i++) {
        if (objs[i].logType == "Stop")
            depth--;
        else {
            depth++;
            if (depth > max_depth)
                max_depth = depth;
        }
    }
    return max_depth;
}

function foo1() {
    foo2();
}
function foo2() {

}

var du = new Debugger();
if (typeof du.drainTraceLoggerScriptCalls == "function") {
    // Test normal setup.
    du = new Debugger();
    du.setupTraceLoggerScriptCalls();

    du.startTraceLogger();
    du.endTraceLogger();

    var objs = du.drainTraceLoggerScriptCalls();
    TestDrainTraceLoggerInvariants(objs);

    // Test basic script.
    for (var i=0; i<20; i++)
        foo1();

    du = new Debugger();
    du.setupTraceLoggerScriptCalls();

    du.startTraceLogger();
    foo1();
    du.endTraceLogger();

    var objs = du.drainTraceLoggerScriptCalls();
    TestDrainTraceLoggerInvariants(objs);
    assertEq(3, GetMaxScriptDepth(objs), "drainTraceLogger.js:0 + foo1 + foo2");
    assertEq(5, objs.length, "drainTraceLogger.js:0 + foo1 + foo2 + stop + stop");

    // Test basic script.
    for (var i=0; i<20; i++)
        foo1();

    du = new Debugger();
    du.setupTraceLoggerScriptCalls();

    du.startTraceLogger();
    for (var i=0; i<100; i++) {
        foo1();
    }
    du.endTraceLogger();

    var objs = du.drainTraceLoggerScriptCalls();
    TestDrainTraceLoggerInvariants(objs);
    assertEq(3, GetMaxScriptDepth(objs), "drainTraceLogger.js:0 + foo1 + foo2");
    assertEq(4*100 + 1, objs.length);
    assertEq(1 + 4*100, objs.length, "drainTraceLogger.js:0 + 4 * ( foo1 + foo2 + stop + stop )");
}
