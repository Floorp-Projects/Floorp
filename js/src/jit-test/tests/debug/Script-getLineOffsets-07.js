// Lazy scripts should correctly report line offsets

var g = newGlobal();
var dbg = new Debugger();

g.eval("// Header comment\n" +   // <- line 6 in this file
       "\n" +
       "\n" +
       "function f(n) {\n" +     // <- line 9 in this file
       "    var foo = '!';\n" +
       "}");

dbg.addDebuggee(g);
var scripts = dbg.findScripts();
for (var i = 0; i < scripts.length; i++) {
  // Nothing should have offsets for the deffun on line 9 if lazy scripts
  // correctly update the position.
  assertEq(scripts[i].getLineOffsets(9).length, 0);
}
