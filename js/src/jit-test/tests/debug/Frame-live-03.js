// |jit-test| debug
// frame.type throws if !frame.live

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var f;");
g.eval("(" + function () {
        Debug(debuggeeGlobal).hooks = {
            debuggerHandler: function (frame) {
                assertEq(frame.type, "call");
                assertEq(frame.live, true);
                f = frame;
            }
        };
    } + ")()");

(function () { debugger; })();
assertEq(g.f.live, false);
assertThrowsInstanceOf(function () { g.f.type; }, g.Error);
