// Each resumption of a generator gets a fresh frame, whose onPop handler
// fires the next time the generator yields.
// This is not the behavior the spec requests, but it's what we do for the
// moment, and it's good to check that at least we don't crash.
var g = newGlobal();
var dbg = new Debugger(g);
var log;

var debuggerFrames = [];
var poppedFrames = [];
dbg.onDebuggerStatement = function handleDebugger(frame) {
    log += 'd';
    assertEq(frame.type, "call");

    assertEq(debuggerFrames.indexOf(frame), -1);
    assertEq(poppedFrames.indexOf(frame), -1);
    debuggerFrames.push(frame);

    if (frame.eval('i').return % 3 == 0) {
        frame.onPop = function handlePop(c) {
            log += ')' + c.return;
            assertEq(debuggerFrames.indexOf(this) != -1, true);
            assertEq(poppedFrames.indexOf(this), -1);
            poppedFrames.push(this);
        };
    }
};

g.eval("function g() { for (var i = 0; i < 10; i++) { debugger; yield i; } }");
log ='';
assertEq(g.eval("var t = 0; for (j in g()) t += j; t;"), 45);
assertEq(log, "d)0ddd)3ddd)6ddd)9");
