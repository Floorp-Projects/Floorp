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

  // Add a hole to all variables. This mustn't affect the implicit rest-argument
  // of the default derived class constructor, otherwise we'll crash when
  // passing the rest-argument to the super spread-call.
  frame.onStep = () => {
    names.forEach(name => {
      var args = frame.environment.getVariable(name);
      if (args && args.deleteProperty) {
        args.deleteProperty(0);
      }
    });
  };
};

new g.Derived(1, 2);

// |Derived| should be called at most once.
assertEq(calleeCount, 1);
