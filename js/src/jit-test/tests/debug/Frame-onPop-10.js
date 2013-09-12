// Setting onPop handlers from an onStep handler works.
var g = newGlobal();
var dbg = new Debugger(g);
var log;

dbg.onDebuggerStatement = function handleDebugger(frame) {
    log += 'd';
    assertEq(frame.type, "eval");
    frame.onStep = function handleStep() {
        log += 's';
        this.onStep = undefined;
        this.onPop = function handlePop() {
            log += ')';
        };
    };
};

log = "";
g.flag = false;
g.eval('debugger; flag = true');
assertEq(log, 'ds)');
assertEq(g.flag, true);
