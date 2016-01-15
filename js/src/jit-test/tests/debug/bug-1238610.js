// |jit-test| allow-oom

if (!('oomAfterAllocations' in this))
    quit();

if (helperThreadCount() === 0)
    quit(0);

lfcode = new Array();
dbg = Debugger();
dbg.onEnterFrame = function() {};
g = newGlobal();
lfcode.push(`
  oomAfterAllocations(100);
  new Number();
  dbg.addDebuggee(g);
`)
file = lfcode.shift();
loadFile(file);
function loadFile(lfVarx) {
  lfGlobal = newGlobal()
  for (lfLocal in this)
      if (!(lfLocal in lfGlobal))
          lfGlobal[lfLocal] = this[lfLocal]
  offThreadCompileScript(lfVarx)
  lfGlobal.runOffThreadScript()
}
