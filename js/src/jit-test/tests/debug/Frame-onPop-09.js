// Setting onPop handlers from an onExceptionUnwind handler works.
var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var log;

dbg.onExceptionUnwind = function handleUnwind(frame) {
    log += 'u';
    assertEq(frame.type, "eval");
    frame.onPop = function handleCallPop(c) {
        log += ')';
        assertEq(c.throw, 'up');
    };
};

log = "";
try {
    g.eval("throw 'up';");
    log += '-';
} catch (x) {
    log += 'c';
    assertEq(x, 'up');
}
assertEq(log, 'u)c');
