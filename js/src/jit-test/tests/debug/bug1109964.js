var dbgGlobal = newGlobal({newCompartment: true});
var dbg = new dbgGlobal.Debugger();
dbg.addDebuggee(this);
function f() {
  var a = arguments;
  a[1];
  dbg.getNewestFrame().eval("a");
}
f();

