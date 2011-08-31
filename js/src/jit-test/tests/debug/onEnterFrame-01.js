// Basic enterFrame hook tests.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var type;
dbg.onEnterFrame = function (frame) {
    try {
        assertEq(frame instanceof Debugger.Frame, true);
        assertEq(frame.live, true);
        type = frame.type;
    } catch (exc) {
        type = "Exception thrown: " + exc;
    }
};

function test(f, expected) {
    type = undefined;
    f();
    assertEq(type, expected);
}

// eval triggers the hook
test(function () { g.eval("function h() { return 1; }"); }, "eval");

// function calls trigger it
test(function () { assertEq(g.h(), 1); }, "call");

// global scripts trigger it
test(function () { g.evaluate("var x = 5;"); assertEq(g.x, 5); }, "global");
