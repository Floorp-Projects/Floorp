// If debugger.onEnterFrame returns undefined, the frame should continue execution.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var hits = 0;
var savedFrame;
dbg.onEnterFrame = function (frame) {
    hits++;
    savedFrame = frame;
    return undefined;
};

savedFrame = undefined;
assertEq(g.eval("'pass';"), "pass");
assertEq(savedFrame.onStack, false);
assertEq(hits, 1);
