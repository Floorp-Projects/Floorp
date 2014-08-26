// getColumnOffsets correctly places array properties.

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
global.eval("function f(n){var a=[1,2,n]} debugger;");
global.f(3);
// Should hit each item in the array.
assertEq(global.log, "18 21 23 25 19 ");
