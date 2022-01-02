
let g = newGlobal({newCompartment: true});
let d = new Debugger(g);

g.eval("")
let script = d.findScripts()[0];

nukeAllCCWs()

// A DeadProxyObject accessing the source-object should not crash.
try {
  let element = script.source.element;
} catch (e) {
}
