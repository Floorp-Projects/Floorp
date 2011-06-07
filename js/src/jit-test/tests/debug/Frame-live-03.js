// |jit-test| debug
// frame properties throw if !frame.live

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var f;");
g.eval("(" + function () {
        Debug(debuggeeGlobal).hooks = {
            debuggerHandler: function (frame) {
                assertEq(frame.live, true);
                assertEq(frame.type, "call");
                assertEq(frame.this instanceof Object, true);
                assertEq(frame.older instanceof Debug.Frame, true);
                assertEq(frame.callee instanceof Debug.Object, true);
                assertEq(frame.generator, false);
                assertEq(frame.constructing, false);
                assertEq(frame.arguments.length, 0);
                f = frame;
            }
        };
    } + ")()");

(function () { debugger; }).call({});
assertEq(g.f.live, false);
assertThrowsInstanceOf(function () { g.f.type; }, g.Error);
assertThrowsInstanceOf(function () { g.f.this; }, g.Error);
assertThrowsInstanceOf(function () { g.f.older; }, g.Error);
assertThrowsInstanceOf(function () { g.f.callee; }, g.Error);
assertThrowsInstanceOf(function () { g.f.generator; }, g.Error);
assertThrowsInstanceOf(function () { g.f.constructing; }, g.Error);
assertThrowsInstanceOf(function () { g.f.arguments; }, g.Error);
