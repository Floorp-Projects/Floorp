// getColumnOffsets correctly places comma separated expressions.

var global = newGlobal();
Debugger(global).onDebuggerStatement = function (frame) {
    var script = frame.eval("f").return.script;
    script.getAllColumnOffsets().forEach(function (offset) {
        script.setBreakpoint(offset.offset, {
            hit: function (frame) {
                assertEq(offset.lineNumber, 1);
                global.log += offset.columnNumber + " ";
            }
        });
    });
};

global.log = '';
global.eval("function f(n){print(n),print(n),print(n)} debugger;");
global.f(3);
// Should hit each call that was separated by commas.
assertEq(global.log, "14 23 32 40 ");
