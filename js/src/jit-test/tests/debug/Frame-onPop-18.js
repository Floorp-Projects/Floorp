// A garbage collection in the debugger compartment does not disturb onPop
// handlers.
var g = newGlobal();
var dbg = new Debugger(g);
var log;

dbg.onEnterFrame = function handleEnter(frame) {
    log += '(';
    frame.onPop = function handlePop(completion) {
        log += ')';
    };
};

dbg.onDebuggerStatement = function handleDebugger (frame) {
    log += 'd';
    // GC in the debugger's compartment only.
    gc(dbg);
};

log = '';
assertEq(g.eval('debugger; 42;'), 42);
assertEq(log, '(d)');
