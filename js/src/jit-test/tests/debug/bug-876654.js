// |jit-test| 
// Exercise finding a DebuggerSource cross compartment wrapper in
// Compartment::findOutgoingEdges()

let g = newGlobal();
let dbg = new Debugger(g);
dbg.onNewScript = function (script) {
  var text = script.source.text;
}
g.eval("function f() { function g() {} }");
gczeal(9, 1)
var a = {}
var b = {}
