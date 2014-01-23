var g = newGlobal();
var dbg = new Debugger(g);
try { g.eval('function drag(ev) {'); } catch (ex) { }
for (s of dbg.findScripts())
  s.lineCount;
