// Environments of different instances of the same generator have the same
// callee. I love this job.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

g.eval('function f(x) { debugger; yield x; }');
g.eval('var g = f(2);');
g.eval('var h = f(3);');

function check(gen, label) {
  print("check(" + label + ")");
  var hits;
  dbg.onDebuggerStatement = function (frame) {
    hits++;
    var env = frame.environment;
    assertEq(env.callee, gw.makeDebuggeeValue(g.f));
  };
  hits = 0;
  gen.next();
  assertEq(hits, 1);
}

check(g.g, 'g.g');
check(g.h, 'g.h');
