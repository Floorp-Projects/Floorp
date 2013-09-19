// Each resumption of an ES6 generator gets a fresh frame, whose onPop
// handler fires the next time the generator yields.  This is not the
// behavior the spec requests, but it's what we do for the moment, and
// it's good to check that at least we don't crash.

load(libdir + 'iteration.js');

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
            log += ')' + c.return.value;
            assertEq(debuggerFrames.indexOf(this) != -1, true);
            assertEq(poppedFrames.indexOf(this), -1);
            poppedFrames.push(this);
        };
    }
};

g.eval("function* g() { for (var i = 0; i < 10; i++) { debugger; yield i; } }");
log ='';
g.eval("var t = 0, iter = g();");
for (var j = 0; j < 10; j++)
    g.eval("t += iter.next().value;");
assertIteratorResult(g.eval("iter.next()"), undefined, true);
assertEq(g.eval("t"), 45);

// FIXME: Should equal this, but see bug 917809.
// assertEq(log, "d)0ddd)3ddd)6ddd)9");
assertEq(log, "d)undefinedddd)undefinedddd)undefinedddd)undefined");
