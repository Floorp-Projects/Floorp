// Each resumption of a generator gets the same Frame; its onPop handler
// fires each time the generator yields.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
var log;

var seenFrame = null;
dbg.onDebuggerStatement = function handleDebugger(frame) {
    log += 'd';
    assertEq(frame.type, "call");

    if (seenFrame === null) {
        seenFrame = frame;
    } else {
        assertEq(seenFrame, frame);
    }

    let i = frame.eval('i').return;
    if (i % 3 == 0) {
        frame.onPop = function handlePop(c) {
            assertEq(this, seenFrame);
            log += ')' + i;
        };
    }
};

g.eval("function* g() { for (var i = 0; i < 10; i++) { debugger; yield i; } }");
log ='';
assertEq(g.eval("var t = 0; for (j of g()) t += j; t;"), 45);
assertEq(log, "d)0d)0d)0d)3d)3d)3d)6d)6d)6d)9)9");
