// |jit-test| exitstatus: 3; skip-if: getBuildConfiguration('pbl')

let debuggerRealm = newGlobal({newCompartment: true});
debuggerRealm.debuggee = this;
debuggerRealm.eval(`
  let dbg = new Debugger(debuggee);
  dbg.onDebuggerStatement = (frame) => null;  // terminate the debuggee
`);

Atomics.add(new Int32Array(1), 0, {
  valueOf() {
    debugger;
  }
});
