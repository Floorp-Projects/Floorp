// uncaughtExceptionHook resumption value other than undefined causes further
// hooks to be skipped.

var g = newGlobal('new-compartment');
var log;

function makeDebug(g, name) {
    var dbg = new Debugger(g);
    dbg.onDebuggerStatement = function (frame) {
        log += name;
        throw new Error(name);
    };
    dbg.uncaughtExceptionHook = function (exc) {
        assertEq(exc.message, name);
        return name == "2" ? {return: 42} : undefined;
    };
}

var arr = [];
for (var i = 0; i < 6; i++)
    arr[i] = makeDebug(g, "" + i);

log = '';
assertEq(g.eval("debugger;"), 42);
assertEq(log, "012");
