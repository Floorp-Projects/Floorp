// getColumnOffsets correctly places the various parts of a ForStatement.

var global = newGlobal();
Debugger(global).onDebuggerStatement = function (frame) {
    var script = frame.eval("f").return.script;
    script.getAllColumnOffsets().forEach(function (offset) {
        script.setBreakpoint(offset.offset, {
            hit: function (frame) {
                assertEq(offset.lineNumber, 17);
                global.log += offset.columnNumber + " ";
            }
        });
    });
};

global.log = '';
global.eval("function f(n) { for (var i = 0; i < n; ++i) log += '. '; log += '! '; } debugger;");
global.f(3);
assertEq(global.log, "21 32 44 . 39 32 44 . 39 32 44 . 39 32 57 ! 69 ");
