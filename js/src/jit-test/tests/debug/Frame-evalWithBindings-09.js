// evalWithBindings code is debuggee code, so it can trip the debugger. It nests!
var g = newGlobal();
var dbg = new Debugger(g);
var f1;
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    f1 = frame;

    // This trips the onExceptionUnwind hook.
    var x = frame.evalWithBindings("wrongSpeling", {rightSpelling: 2}).throw;

    assertEq(frame.evalWithBindings("exc.name", {exc: x}).return, "ReferenceError");
    hits++;
};
dbg.onExceptionUnwind = function (frame, exc) {
    assertEq(frame !== f1, true);

    // f1's environment does not contain the binding for the first evalWithBindings call.
    assertEq(f1.eval("rightSpelling").return, "dependent");
    assertEq(f1.evalWithBindings("n + rightSpelling", {n: "in"}).return, "independent");

    // frame's environment does contain the binding.
    assertEq(frame.eval("rightSpelling").return, 2);
    assertEq(frame.evalWithBindings("rightSpelling + three", {three: 3}).return, 5);
    hits++;
};
g.eval("(function () { var rightSpelling = 'dependent'; debugger; })();");
