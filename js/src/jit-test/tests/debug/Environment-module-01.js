// Debug environments for module environments should include variables that are
// are not closed over or exported.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.onEnterFrame = function (frame) {
  if (!frame.older) {
    return;
  }
  const env = frame.older.environment;
  assertEq(env.names().join(','), "foo,y,x,z");
  assertEq(env.getVariable('x'), 0);
  assertEq(env.getVariable('y'), 1);
  assertEq(env.getVariable('z'), 2);
  env.setVariable('x', 3);
  assertEq(env.getVariable('x'), 3);
};
const m = g.parseModule(`
  var x = 0;
  export var y = 1;
  const z = 2;
  foo();
  function foo() {}
`);
moduleLink(m);
moduleEvaluate(m);
