var g = newGlobal();
var dbg = new Debugger(g);

try {
  g.eval("function f() { var array = ['a', 'b']; [1].map(function () {}); return {array}; }");
} catch (e) {
  // Ignore the syntax error.
}

dbg.findScripts();
