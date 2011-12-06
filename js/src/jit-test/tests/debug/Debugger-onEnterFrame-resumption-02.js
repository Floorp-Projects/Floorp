// If debugger.onEnterFrame returns {throw:val}, an exception is thrown in the debuggee.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
g.set = false;
g.eval("function f() {\n" +
       "    set = true;\n" +
       "    return 'fail';\n" +
       "}\n");

var dbg = Debugger(g);
var savedFrame;
dbg.onEnterFrame = function (frame) {
    savedFrame = frame;
    return {throw: "pass"};
};

savedFrame = undefined;
assertThrowsValue(g.f, "pass");
assertEq(savedFrame.live, false);
assertEq(g.set, false);

savedFrame = undefined;
assertThrowsValue(function () { new g.f; }, "pass");
assertEq(savedFrame.live, false);
assertEq(g.set, false);

