// Creating a new generator frame after the generator is closed.

var g = newGlobal({ newCompartment: true });
g.eval("function* gen(x) { debugger; }");
var dbg = new Debugger(g);
dbg.onDebuggerStatement = frame => {
  frame.onPop = completion => {
    assertEq(frame.callee.name, "gen");
    assertEq(frame.eval("x").return, 3);
    var f2 = (new Debugger(g)).getNewestFrame();
    assertEq(f2.callee.name, "gen");
    assertEq(f2.eval("x").return, 3);
  };
};
g.gen(3).next();

