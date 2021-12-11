// Environments of different instances of the same generator have the same
// calleeScript.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

function check(gen, label) {
  print("check(" + label + ")");
  var hits;
  dbg.onDebuggerStatement = function (frame) {
    hits++;
    var env = frame.environment;
    assertEq(env.calleeScript, gw.makeDebuggeeValue(g.f).script);
  };
  hits = 0;
  gen.next();
  assertEq(hits, 1);
}

g.eval('function* f(x) { debugger; yield x; }');
g.eval('var g = f(2);');
g.eval('var h = f(3);');
check(g.g, 'g.g');
check(g.h, 'g.h');
