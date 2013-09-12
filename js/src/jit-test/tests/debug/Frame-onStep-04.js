// When a recursive function has many frames on the stack, onStep may be set or
// not independently on each frame.

var g = newGlobal();
g.eval("function f(x) {\n" +
       "    if (x > 0)\n" +
       "        f(x - 1);\n" +
       "    else\n" +
       "        debugger;\n" +
       "    return x;\n" +
       "}");

var dbg = Debugger(g);
var seen = [0, 0, 0, 0, 0, 0, 0, 0];
function step() {
    seen[this.arguments[0]] = 1;
}
dbg.onEnterFrame = function (frame) {
    // Turn on stepping for even-numbered frames.
    var x = frame.arguments[0];
    if (x % 2 === 0)
        frame.onStep = step;
};
dbg.onDebuggerStatement = function (frame) {
    // This is called with 8 call frames on the stack, 7 down to 0.
    // At this point we should have seen all the even-numbered frames.
    assertEq(seen.join(""), "10101010");

    // Now reset seen to see which frames fire onStep on the way out.
    seen = [0, 0, 0, 0, 0, 0, 0, 0];
};

g.f(7);
assertEq(seen.join(""), "10101010");
