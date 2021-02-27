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

  const names = frame.environment.names();

  // Set all variables to |null|. This mustn't affect the implicit rest-argument
  // of the default derived class constructor, otherwise we'll crash when
  // passing the rest-argument to the super spread-call.
  frame.onStep = () => {
    names.forEach(name => {
      frame.environment.setVariable(name, null);
    });
  };
};

new g.Derived();

// |Derived| should be called at most once.
assertEq(calleeCount, 1);
