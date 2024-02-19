// |jit-test| --more-compartments; error:ReferenceError

gczeal(19, 5)
evaluate(`
  a = newGlobal()
  Debugger(a).onDebuggerStatement = function () { b };
  a.eval("var line0 = Error().lineNumber\\ndebugger\\n" )
`);
gczeal(15);
gczeal(10, 2)();
