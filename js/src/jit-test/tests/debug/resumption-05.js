// |jit-test| debug
// null resumption value means terminate the debuggee

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("(" + function () { 
        var dbg = new Debugger(debuggeeGlobal);
        dbg.onDebuggerStatement = function (frame) {
            if (frame.callee === null) {
                // The first debugger statement below.
                debuggeeGlobal.log += "1";
                var cv = frame.eval("f();");
                assertEq(cv, null);
                debuggeeGlobal.log += "2";
            } else {
                // The second debugger statement.
                debuggeeGlobal.log += "3";
                assertEq(frame.callee.name, "f");
                return null;
            }
        };
    } + ")()");

var log = "";
debugger;

function f() {
    log += "4";
    try {
        debugger;  // the debugger terminates us here
    } finally {
        log += "5";  // this should not execute
    }
}

assertEq(log, "1432");
