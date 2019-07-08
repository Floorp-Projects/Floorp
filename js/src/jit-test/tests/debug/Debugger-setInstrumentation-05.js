// Test some inputs that should fail instrumentation validation.

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

function assertThrows(fn, text) {
  try {
    fn();
    assertEq(true, false);
  } catch (e) {
    if (!e.toString().includes(text)) {
      print(`Expected ${text}, got ${e}`);
      assertEq(true, false);
    }
  }
}

assertThrows(() => gdbg.setInstrumentation(undefined, []),
            "Instrumentation callback must be an object");
assertThrows(() => gdbg.setInstrumentation(gdbg.makeDebuggeeValue({}), ["foo"]),
            "Unknown instrumentation kind");

let lastScript = null;
dbg.onNewScript = script => lastScript = script;

assertThrows(() => {
  g.eval(`x = 3`);
  lastScript.setInstrumentationId("twelve");
}, "Script instrumentation ID must be a number");

assertThrows(() => {
  g.eval(`x = 3`);
  lastScript.setInstrumentationId(10);
  lastScript.setInstrumentationId(11);
}, "Script instrumentation ID is already set");

dbg.onNewScript = undefined;
gdbg.setInstrumentation(gdbg.makeDebuggeeValue({}), ["main"]);
gdbg.setInstrumentationActive(true);

assertThrows(() => { g.eval(`x = 3`) },
             "Instrumentation ID not set for script");
