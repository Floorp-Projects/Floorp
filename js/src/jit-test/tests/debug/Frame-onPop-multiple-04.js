// If one Debugger's onPop handler causes another Debugger to create a
// Debugger.Frame instance referring to the same frame, that frame still
// gets marked as not live after all the onPop handlers have run.
var g = newGlobal('new-compartment');
var dbg1 = new Debugger(g);
var dbg2 = new Debugger(g);

var log;
var frame2;

dbg1.onEnterFrame = function handleEnter(frame) {
    log += '(';
    frame.onPop = function handlerPop1(c) {
        log += ')';
        frame2 = dbg2.getNewestFrame();
        assertEq(frame2.live, true);
        frame2.onPop = function handlePop2(c) {
            assertEq("late frame's onPop handler ran",
                     "late frame's onPop handler should not run");
        };
    };
};

log = '';
assertEq(g.eval('40 + 2'), 42);
assertEq(log, '()');
assertEq(frame2.live, false);
