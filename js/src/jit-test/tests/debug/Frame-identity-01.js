// Check that {return:} resumption kills the current stack frame.

var g = newGlobal({newCompartment: true});
g.debuggeeGlobal = this;
g.eval("(" + function () {
        var dbg = new Debugger(debuggeeGlobal);
        var prev = null;
        dbg.onDebuggerStatement = function (frame) {
            assertEq(frame === prev, false);
            if (prev)
                assertEq(prev.onStack, false);
            prev = frame;
            return {return: frame.arguments[0]};
        };
    } + ")();");

function f(i) { debugger; }
for (var i = 0; i < 10; i++)
    assertEq(f(i), i);
