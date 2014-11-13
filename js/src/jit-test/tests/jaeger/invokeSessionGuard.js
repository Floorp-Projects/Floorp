var g = newGlobal();
var dbg = new g.Debugger(this);
// Force dbg to observe all execution.
dbg.onDebuggerStatement = function () {};

[1,2,3,4,5,6,7,8].forEach(
    function(x) {
        // evalInFrame means lightweight gets call obj
        assertEq(evalInFrame(0, "x"), x);
    }
);
