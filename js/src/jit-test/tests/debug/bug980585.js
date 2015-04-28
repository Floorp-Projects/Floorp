var g = newGlobal();
var dbg = new Debugger(g);

try {
  g.eval("function f() { [1].map(function () {}); const x = 42; x = 43; } f();");
} catch (e) {
  // Ignore the syntax error.
}

dbg.findScripts();
