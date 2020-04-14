// Test that instrumenting a function does not interfere with script
// disassembly when reporting errors.

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

function setScriptId(script) {
  script.setInstrumentationId(0);
  script.getChildScripts().forEach(setScriptId);
}

dbg.onNewScript = setScriptId;

const executedLines = [];
gdbg.setInstrumentation(
  gdbg.makeDebuggeeValue(() => {}),
  ["breakpoint"]
);

g.eval(`
function foo(v) {
  v.f().g();
}
`);

var gotException = false;
try {
  g.foo({ f: () => { return { g: 0 } } });
} catch (e) {
  gotException = true;
  assertEq(e.toString().includes("v.f().g is not a function"), true);
}
assertEq(gotException, true);
