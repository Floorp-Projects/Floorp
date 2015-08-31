// Debugger.Environment.prototype.callee reveals the callee of environments
// that have them.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

function check(code, expectedType, expectedCallee) {
  print("check(" + uneval(code) + ")");
  var hits;
  dbg.onDebuggerStatement = function (frame) {
    hits++;
    var env = frame.environment;
    assertEq(env.type, expectedType);
    assertEq(env.callee, expectedCallee);
  };
  hits = 0;
  g.eval(code);
  assertEq(hits, 1);
}

check('debugger;', 'declarative', null);
check('with({}) { debugger; };', 'with', null);
check('let (x=1) { debugger; };', 'declarative', null);

g.eval('function f() { debugger; }');
check('f()', 'declarative', gw.makeDebuggeeValue(g.f));

g.eval('function g() { h(); }');
g.eval('function h() { debugger; }');
check('g()', 'declarative', gw.makeDebuggeeValue(g.h));

// All evals get a lexical scope.
check('"use strict"; eval("debugger");', 'declarative', null);
g.eval('function j() { "use strict"; eval("debugger;"); }');
check('j()', 'declarative', null);

// All evals get a lexical scope.
check('eval("debugger");', 'declarative', null);

g.eval('function m() { debugger; yield true; }');
check('m().next();', 'declarative', gw.makeDebuggeeValue(g.m));

g.eval('function n() { let (x = 1) { debugger; } }');
check('n()', 'declarative', null);

g.eval('function* o() { debugger; yield true; }');
check('o().next();', 'declarative', gw.makeDebuggeeValue(g.o));
