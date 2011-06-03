// |jit-test| debug
// frame.type throws if !frame.live

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var f;
Debug(g).hooks = {
    debuggerHandler: function (frame) {
        assertEq(frame.type, "call");
        assertEq(frame.live, true);
        f = frame;
    }
};

g.eval("(function () { debugger; })();");
assertEq(f.live, false);
assertThrowsInstanceOf(function () { f.type; }, Error);
