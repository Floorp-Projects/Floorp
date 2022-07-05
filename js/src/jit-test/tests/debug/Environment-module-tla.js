// Debug environments for module environments should include variables that are
// are not closed over or exported, even after top-level-await.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (frame) {
  const env = frame.environment;
  assertEq(env.names().join(','), "y,x,z");
  assertEq(env.getVariable('x'), 0);
  assertEq(env.getVariable('y'), 1);
  assertEq(env.getVariable('z'), 2);
};
const m = g.parseModule(`
  var x = 0;
  export var y = 1;
  const z = 2;
  debugger;
  await 10;
  debugger;
  await 10;
  debugger;
`);
moduleLink(m);
moduleEvaluate(m);
drainJobQueue();
