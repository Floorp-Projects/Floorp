const g = newGlobal({newCompartment: true});
g.eval(`
  class Base {}
  class Derived extends Base {}
  this.Derived = Derived;
`);

const dbg = new Debugger(g);
const gw = dbg.addDebuggee(g);

let calleeCount = 0;

dbg.onEnterFrame = frame => {
  // Ignore any other callees.
  if (frame.callee !== gw.makeDebuggeeValue(g.Derived)) {
    return;
  }

  calleeCount++;

  // The implicit rest-argument has the non-identifier name ".args", therefore
  // we have to elide from the parameter names array.
  assertEq(frame.callee.parameterNames.length, 1);
  assertEq(frame.callee.parameterNames[0], undefined);
};

new g.Derived();

// |Derived| should be called at most once.
assertEq(calleeCount, 1);
