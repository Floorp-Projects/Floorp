// |jit-test| error: Error

var g = newGlobal();
g.eval('function f(a) { if (a == 1) debugger; evaluate("f(" + a + " - 1);"); }');
var N = 2;
var dbg = new Debugger(g);
var frames = [];
dbg.onEnterFrame = function (frame) {
   frames.push(frame);
   frame.onPop = function () { assertEq(frame.onPop, frame.onPop); };
};
dbg.onDebuggerStatement = function (frame) {
    for (var f of frames)
        f.eval('a').return;
};
evaluate("g.f(N);");
