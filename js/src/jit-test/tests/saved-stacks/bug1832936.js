const g = newGlobal({"newCompartment": true});
const dbg = Debugger(g);

dbg.onDebuggerStatement = function () {
  const stack = saveStack();
  for (let i = 0; i < 50; i++) {}
  function foo() {
    saveStack();
    dbg.getNewestFrame().eval(`saveStack()`);
  }
  bindToAsyncStack(foo, {"stack": stack})();
}
g.eval("debugger;");
