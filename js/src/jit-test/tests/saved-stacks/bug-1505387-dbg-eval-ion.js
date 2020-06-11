// |jit-test| --ion-eager; --no-threads;
// This test ensures that debugger eval on an ion frame is able to correctly
// follow the debugger eval frame link to its parent frame.

var dbgGlobal = newGlobal({ newCompartment: true });
var dbg = new dbgGlobal.Debugger(globalThis);

for (var i = 0; i < 1; i++) {
  (() => {
    dbg.getNewestFrame().older.eval("saveStack()");
  })();
}
