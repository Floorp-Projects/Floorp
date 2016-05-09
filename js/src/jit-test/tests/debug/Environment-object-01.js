var g = newGlobal();
var dbg = new Debugger(g);

dbg.onDebuggerStatement = (frame) => {
  assertEq(frame.environment.parent.type, "object");
  assertEq(frame.environment.parent.object.getOwnPropertyDescriptor("x").value, 42);
}
g.evalReturningScope("x = 42; debugger;");
