function TestDrainTraceLoggerInvariants(obj) {
    var scripts = 0;
    var stops = 0;
    for (var i = 0; i < objs.length; i++) {
        if (objs[i].logType == "Scripts") {
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
    assertEq(scripts, stops);
}

function GetMaxScriptDepth(obj) {
    var max_depth = 0;
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
if (typeof du.drainTraceLoggerTraces == "function") {
print(1);
    // Test normal setup.
    du = new Debugger();
    du.setupTraceLoggerForTraces();

    du.startTraceLogger();
    du.endTraceLogger();

    var objs = du.drainTraceLoggerTraces();
    TestDrainTraceLoggerInvariants(objs);
    var empty_depth = GetMaxScriptDepth(objs);
    var empty_length = objs.length;

    // Test basic script.
    for (var i=0; i<20; i++)
        foo1();

    du = new Debugger();
    du.setupTraceLoggerTraces();

    du.startTraceLogger();
    foo1();
    du.endTraceLogger();

    var objs = du.drainTraceLoggerTraces();
    TestDrainTraceLoggerInvariants(objs);
    assertEq(empty_depth + 2 == GetMaxScriptDepth(objs));
    assertEq(empty_length + 4 == GetMaxScriptDepth(objs));
    
    // Test basic script.
    for (var i=0; i<20; i++)
        foo1();

    du = new Debugger();
    du.setupTraceLoggerForTraces();

    du.startTraceLogger();
    for (var i=0; i<100; i++) {
        foo1();
    }
    du.endTraceLogger();

    var objs = du.drainTraceLoggerTraces();
    TestDrainTraceLoggerInvariants(objs);
    assertEq(empty_depth + 2 == GetMaxScriptDepth(objs));
    assertEq(empty_length + 4*100 == GetMaxScriptDepth(objs));
}
