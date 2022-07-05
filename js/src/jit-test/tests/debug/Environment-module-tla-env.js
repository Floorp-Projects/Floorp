// Taking frame snapshot after await should not crash.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
  // Create DebugEnvironmentProxy.
  frame.environment;
};
const m = g.parseModule(`
  var x = 0;
  debugger;
  await 10;
  debugger;
`);
moduleLink(m);
moduleEvaluate(m);
