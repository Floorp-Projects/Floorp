// If debugger.onEnterFrame returns {return:val}, the frame returns immediately.

load(libdir + "asserts.js");

var g = newGlobal();
g.set = false;

var dbg = Debugger(g);
var savedFrame;
dbg.onDebuggerStatement = function (frame) {
    var innerSavedFrame;
    dbg.onEnterFrame = function (frame) {
        innerSavedFrame = frame;
        return null;
    };
    // Using frame.eval lets us catch termination.  
    assertEq(frame.eval("set = true;"), null);
    assertEq(innerSavedFrame.live, false);
    savedFrame = frame;
    return { return: "pass" };
};

savedFrame = undefined;
assertEq(g.eval("debugger;"), "pass");
assertEq(savedFrame.live, false);
assertEq(g.set, false);
