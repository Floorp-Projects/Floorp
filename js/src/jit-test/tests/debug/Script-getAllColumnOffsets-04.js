// getColumnOffsets correctly places object properties.

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
global.eval("function f(n){var o={a:1,b:2,c:3}} debugger;");
global.f(3);
// Should hit each property in the object.
assertEq(global.log, "18 21 25 29 19 ");
