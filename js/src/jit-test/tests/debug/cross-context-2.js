// The debugger can eval in any frame in the stack even if every frame was
// pushed in a different context.

var g = newGlobal();
g.eval('function f(a) {\n' +
       '    if (a == 1)\n' +
       '        debugger;\n' +
       '    else\n' +
       '        evaluate("f(" + a + " - 1);", {newContext: true});\n' +
       '}\n');
var N = 9;

var dbg = new Debugger(g);
var frames = [];
var hits = 0;
dbg.onEnterFrame = function (frame) {
    if (frame.type == "call" && frame.callee.name == "f") {
        frames.push(frame);
        frame.onPop = function () { assertEq(frames.pop(), frame); };
    }
};
dbg.onDebuggerStatement = function (frame) {
    assertEq(frames.length, N);
    var i = N;
    for (var f of frames)
        assertEq(f.eval('a').return, i--);
    hits++;
};

g.f(N);
assertEq(hits, 1);
assertEq(frames.length, 0);
