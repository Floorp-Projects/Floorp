// A frame's onPop handler is called only once, even if it is for a function
// called from a loop.
var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var log;

var count;
dbg.onDebuggerStatement = function handleDebug(frame) {
    log += 'd';
    assertEq(frame.type, "call");
    count++;
    if (count == 10) {
        frame.onPop = function handlePop(c) {
            log += ')' + this.arguments[0];
            assertEq(c.return, "snifter");
        };
    }
};

g.eval("function f(n) { debugger; return 'snifter'; }");
log = '';
count = 0;
g.eval("for (i = 0; i < 20; i++) f(i);");
assertEq(count, 20);
assertEq(log, "dddddddddd)9dddddddddd");
