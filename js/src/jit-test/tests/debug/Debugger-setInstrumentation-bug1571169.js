// |jit-test| error: Error

var lfOffThreadGlobal = newGlobal();
evaluate(`
  nukeAllCCWs();
  var g92 = newGlobal({ newCompartment: true });
  var dbg = Debugger(g92);
  var gdbg = dbg.addDebuggee(g92);
  gdbg.setInstrumentation(
    gdbg.makeDebuggeeValue((kind, script, offset) => {}),
    ["breakpoint"]
  );
  gdbg.setInstrumentationActive(true);
  g92.eval(\`
    function basic() {}
  \`);
`);

