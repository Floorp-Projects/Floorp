// The onExceptionUnwind hook is called multiple times as the stack unwinds.

var g = newGlobal();
g.debuggeeGlobal = this;
g.dbg = null;
g.eval("(" + function () {
        dbg = new Debugger(debuggeeGlobal);
        dbg.onExceptionUnwind = function (frame, exc) {
            assertEq(frame instanceof Debugger.Frame, true);
            assertEq(exc instanceof Debugger.Object, true);
            var s = '!';
            for (var f = frame; f; f = f.older)
                if (f.type === "call")
                    s += f.callee.name;
            s += ', ';
            debuggeeGlobal.log += s;
        };
    } + ")();");

var log;

function k() {
    try {
        throw new Error("oops");  // hook call 1
    } finally {
        log += 'k-finally, ';
    } // hook call 2
}

function j() {
    k();  // hook call 3
    log += 'j-unreached, ';
}

function h() {
    try {
        j();  // hook call 4
        log += 'h-unreached, ';
    } catch (exc) {
        log += 'h-catch, ';
        throw exc; // hook call 5
    }
}

function f() {
    try {
        h(); // hook call 6
    } catch (exc) {
        log += 'f-catch, ';
    }
    log += 'f-after, ';
}

log = '';
f();
g.dbg.enabled = false;
assertEq(log, '!kjhf, k-finally, !kjhf, !jhf, !hf, h-catch, !hf, !f, f-catch, f-after, ');
