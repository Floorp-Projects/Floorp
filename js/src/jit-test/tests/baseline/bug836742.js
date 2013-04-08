// |jit-test| debug
// Ensure the correct frame is passed to exception unwind hooks.
var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("(" + function () {
    frames = [];
    var dbg = Debugger(debuggeeGlobal);
    dbg.onEnterFrame = function(frame) {
	frames.push(frame);
    };
    dbg.onExceptionUnwind = function(frame) {
	assertEq(frames.indexOf(frame), frames.length - 1);
	frames.pop();
	assertEq(frame, dbg.getNewestFrame());
    }
} + ")()");

function f(n) {
    debugger;
    n--;
    if (n > 0) {
        f(n);
    } else {
	assertEq(g.frames.length, 10);
        throw "fit";
    }
}
try {
    f(10);
    assertEq(0, 1);
} catch (e) {
    assertEq(e, "fit");
}
assertEq(g.frames.length, 0);
