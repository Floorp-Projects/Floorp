// Test extracting frame.arguments element getters and calling them in
// various awkward ways.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
var fframe, farguments, fgetter;
dbg.onDebuggerStatement = function (frame) {
    if (hits === 0) {
        fframe = frame;
        farguments = frame.arguments;
        fgetter = Object.getOwnPropertyDescriptor(farguments, "0").get;
        assertEq(fgetter instanceof Function, true);

        // Calling the getter without an appropriate this-object
        // fails, but shouldn't assert or crash.
        assertThrowsInstanceOf(function () { fgetter.call(Math); }, TypeError);
    } else {
        // Since fframe is still on the stack, fgetter can be applied to it.
        assertEq(fframe.live, true);
        assertEq(fgetter.call(farguments), 100);

        // Since h was called without arguments, there is no argument 0.
        assertEq(fgetter.call(frame.arguments), undefined);
    }
    hits++;
};

g.eval("function h() { debugger; }");
g.eval("function f(x) { debugger; h(); }");
g.f(100);
assertEq(hits, 2);

// Now that fframe is no longer live, trying to get its arguments should throw.
assertEq(fframe.live, false);
assertThrowsInstanceOf(function () { fgetter.call(farguments); }, Error);
