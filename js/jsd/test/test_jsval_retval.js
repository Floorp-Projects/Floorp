// Bug 689101 - if the binary layout of jsval does not match between C and C++
// code, then calls to functions returning jsval may get compiled differently
// than the callee, resulting in parameters being shifted over by one.
//
// An example is where on Windows, calling jsdValue.getWrappedValue() will
// return a random floating point number instead of an object.
//
// This test must be run with debugging already enabled

function run_test() {
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    const DebuggerService = Cc["@mozilla.org/js/jsd/debugger-service;1"];
    const jsdIDebuggerService = Ci.jsdIDebuggerService;
    var jsd = DebuggerService.getService(jsdIDebuggerService);

    do_check_true(jsd.isOn);

    var n = 0;
    function f() {
        n++;
    }

    jsd.enumerateScripts({ enumerateScript: function(script) {
        script.setBreakpoint(0);
    } });

    jsd.breakpointHook = function(frame, type, dummy) {
        var scope = frame.scope;
        var parent = scope.jsParent; // Probably does not need to be called
        var wrapped = scope.getWrappedValue();
        // Do not try to print 'wrapped'; it may be an internal Call object
        // that will crash when you toString it. Different bug.
        do_check_eq(typeof(wrapped), "object");
        return Ci.jsdIExecutionHook.RETURN_CONTINUE;
    };

    f();

    jsd.breakpointHook = null;
    jsd = null;
}
