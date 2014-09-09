// Function.prototype's script source should be fully initialized.
var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

var DOfp = gw.getOwnPropertyDescriptor('Function').value.proto;
// This should not crash.
print(DOfp.script.source.introductionScript);
