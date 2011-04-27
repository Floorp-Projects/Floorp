// |jit-test| debug
// When the debugger is triggered twice from the same stack frame, the same
// Debug.Frame object must be passed to the hook both times.

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var hits, frame;");
g.eval("(" + function () {
        var dbg = Debug(debuggeeGlobal);
        dbg.hooks = {
            debuggerHandler: function (f) {
                if (hits++ == 0)
                    frame = f;
                else
                    assertEq(f, frame);
            }
        };
    } + ")()");

g.hits = 0;
debugger;
debugger;
assertEq(g.hits, 2);

g.hits = 0;
function f() {
    debugger;
    debugger;
}
f();
assertEq(g.hits, 2);

g.hits = 0;
eval("debugger; debugger;");
assertEq(g.hits, 2);
