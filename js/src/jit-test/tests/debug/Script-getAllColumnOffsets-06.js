// getColumnOffsets correctly places function calls.

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

global.log = "";
global.eval("function ppppp() { return 1; }");
//                     1         2         3         4
//           01234567890123456789012345678901234567890123456789
global.eval("function f(){ 1 && ppppp(ppppp()) && new Error() } debugger;");
global.f();

// 14 - Enter the function body
// 25 - Inner print()
// 19 - Outer print()
// 37 - new Error()
// 49 - Exit the function body
assertEq(global.log, "14 25 19 37 49 ");
