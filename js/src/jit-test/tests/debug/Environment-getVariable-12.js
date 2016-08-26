var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    assertEq(frame.environment.parent.getVariable('y'), true);
};

g.eval("var g;" +
       "function f(x) {" +
       "  { let y = x; " +
       "    if (x)" +
       "      g = function() { eval('debugger') };" +
       "    else" +
       "      g();" +
       "  }" +
       "}" +
       "f(true);" +
       "f(false);");
assertEq(hits, 1);

var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    assertEq(frame.environment.parent.getVariable('y'), 1);
    assertEq(frame.environment.parent.names().indexOf('z'), -1);
};

g.eval("var g;" +
       "{ let y = 1; " +
       "  g = function () { debugger; };" +
       "  { let z = 2; " +
       "    g();" +
       "  }"+
       "}");
assertEq(hits, 1);

var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    hits++;
    var e = frame.older.environment.parent;
    assertEq(e.getVariable('z'), true);
    e = e.parent;
    assertEq(e.getVariable('y'), true);
};

g.eval("var g;" +
       "function h() { debugger };" +
       "for (var x of [true, false]) {" +
       "  { let y = x; " +
       "    { let z = x; " +
       "      if (x)" +
       "        g = function () { print(z); h() };" +
       "      else" +
       "        g();" +
       "    }" +
       "  }" +
       "}");
assertEq(hits, 1);
