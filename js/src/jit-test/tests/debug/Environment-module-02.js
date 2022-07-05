// Debug environments for module environments should be able to access closed
// over variables after the module script has executed.

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);

const m = g.parseModule(`
  var x = 42;
  export function foo() { return x; }
  foo();
`);
moduleLink(m);

let fooFunction;
dbg.onEnterFrame = function (frame) {
  fooFunction = frame.callee;
};

moduleEvaluate(m);
assertEq(fooFunction instanceof Debugger.Object, true);

dbg.onEnterFrame = function (frame) {
  const env = frame.environment.parent;
  assertEq(env.names().join(','), "foo,x");
  assertEq(env.getVariable('x'), 42);
  env.setVariable('x', 3);
  assertEq(env.getVariable('x'), 3);
};

fooFunction.call();
