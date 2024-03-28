// removeDebuggee can be called through ToPrimitive while converting the argument
// passed to Debugger.Environment.{find,getVariable,setVariable} to string.

var g = newGlobal({newCompartment: true});
g.eval("function f() { debugger; }");
var dbg = new Debugger();
var oddball = {[Symbol.toPrimitive]: () => dbg.removeDebuggee(g)};

for (var method of ["find", "getVariable", "setVariable"]) {
  dbg.addDebuggee(g);
  dbg.onDebuggerStatement = frame => {
    var ex;
    try {
      frame.environment[method](oddball, oddball);
    } catch (e) {
      ex = e;
    }
    assertEq(ex.message, "Debugger.Environment is not a debuggee environment");
  };
  g.f();
}
