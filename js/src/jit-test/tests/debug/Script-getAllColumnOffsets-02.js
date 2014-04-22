// getColumnOffsets correctly places multiple variable declarations.

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
global.eval("function f(n){var w0,x1=3,y2=4,z3=9} debugger;");
global.f(3);

// Should have hit each variable declared.
assertEq(global.log, "18 21 26 31 33 ");
