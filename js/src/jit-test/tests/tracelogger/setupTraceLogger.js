
var du = new Debugger();
if (typeof du.setupTraceLogger == "function") {

    // Try enabling.
    assertEq(du.setupTraceLogger({
        Scripts: true
    }), true);

    // No fail on re-enabling.
    assertEq(du.setupTraceLogger({
        Scripts: true
    }), true);

    // Try disabling.
    assertEq(du.setupTraceLogger({
        Scripts: false
    }), true);

    // No fail on re-disabling.
    assertEq(du.setupTraceLogger({
        Scripts: false
    }), true);

    // Throw exception if TraceLog item to report isn't found.
    var success = du.setupTraceLogger({
        Scripts: false,
        Test: true
    });
    assertEq(success, false);

    // SetupTraceLogger only enables individual items,
    // when all items can be toggled.
    du.startTraceLogger();
    var obj = du.drainTraceLogger();
    du.setupTraceLogger({
        Scripts: true,
        Test: true,
    });
    assertEq(du.drainTraceLogger().length, 0);
    du.endTraceLogger();

    // Expects an object as first argument.
    succes = du.setupTraceLogger("blaat");
    assertEq(succes, false);

    // Expects an object as first argument.
    succes = du.setupTraceLogger("blaat");
    assertEq(succes, false);

    // Expects an object as first argument.
    failed = false;
    try {
        du.setupTraceLogger();
    } catch (e) {
        failed = true;
    }
    assertEq(failed, true);

    // No problem with added to many arguments.
    succes = du.setupTraceLogger({}, "test");
    assertEq(succes, true);
}

var du2 = new Debugger();
if (typeof du2.setupTraceLoggerForTraces == "function") {
    du2.setupTraceLoggerForTraces({});
    du2.setupTraceLoggerForTraces("test");
    du2.setupTraceLoggerForTraces({}, "test");
}
