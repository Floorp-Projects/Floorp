// Setting an onPop handler from an onPop handler doesn't throw, but the
// new handler doesn't fire.
var g = newGlobal();
var dbg = new Debugger(g);
var log;

dbg.onDebuggerStatement = function handleDebugger(frame) {
    log += 'd';
    assertEq(frame.type, "eval");
    frame.onPop = function firstHandlePop(c) {
        log +=')';
        assertEq(c.return, 'on investment');
        this.onPop = function secondHandlePop(c) {
            assertEq("secondHandlePop was called", "secondHandlePop should never be called");
        };
    };
};

log = "";
g.eval("debugger; 'on investment';");
assertEq(log, 'd)');
