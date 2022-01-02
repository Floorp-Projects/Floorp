var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

dbg.onDebuggerStatement = (frame) => {
  assertEq(frame.environment.parent.type, "with");
  assertEq(frame.environment.parent.parent.type, "object");
  assertEq(frame.environment.parent.parent.object.getOwnPropertyDescriptor("x").value, 42);
}
g.evalReturningScope("x = 42; debugger;");
