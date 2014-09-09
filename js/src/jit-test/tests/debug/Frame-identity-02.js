// Check that {throw:} resumption kills the current stack frame.

load(libdir + "asserts.js");

var g = newGlobal();
g.debuggeeGlobal = this;
g.eval("(" + function () {
        var dbg = new Debugger(debuggeeGlobal);
        var prev = null;
        dbg.onDebuggerStatement = function (frame) {
            assertEq(frame === prev, false);
            if (prev)
                assertEq(prev.live, false);
            prev = frame;
            return {throw: debuggeeGlobal.i};
        };
    } + ")();");

function f() { debugger; }
for (var i = 0; i < 10; i++)
    assertThrowsValue(f, i);
