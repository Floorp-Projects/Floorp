// |jit-test| error: ReferenceError
gczeal(0);
setMarkStackLimit(1);
var g = newGlobal({
    newCompartment: true
});
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
dbg.onDebuggerStatement = function(frame) {
  frame.environment.parent.getVariable('y')
};
g.eval(`
  let y = 1;
  g = function () { debugger; };
  g();
`);
gczeal(9, 10);
f4();
