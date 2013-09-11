// Debugger.Environment.prototype.callee gets the right closure.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

g.eval('function f(x) { return function (y) { debugger; return x + y; } }');
g.eval('var g = f(2);');
g.eval('var h = f(3);');

function check(fun, label) {
  print("check(" + label + ")");
  var hits;
  dbg.onDebuggerStatement = function (frame) {
    hits++;
    var env = frame.environment;
    assertEq(env.callee, gw.makeDebuggeeValue(fun));
  };
  hits = 0;
  fun();
  assertEq(hits, 1);
}

check(g.g, 'g.g');
check(g.h, 'g.h');
