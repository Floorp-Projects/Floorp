var g1 = newGlobal({newCompartment: true});

const dbg = new Debugger();

function assertThrowsDeadWrapper(f) {
  let caught = false;
  try {
    f();
  } catch (e) {
    assertEq(e.message, "can't access dead object");
    caught = true;
  }
  assertEq(caught, true);
}

nukeAllCCWs();

// Debugger methods should throw explicit error for dead global object.
assertThrowsDeadWrapper(() => dbg.addDebuggee(g1));
assertThrowsDeadWrapper(() => dbg.removeDebuggee(g1));
assertThrowsDeadWrapper(() => dbg.findScripts({global: g1}));
assertThrowsDeadWrapper(() => dbg.makeGlobalObjectReference(g1));
assertThrowsDeadWrapper(() => dbg.enableAsyncStack(g1));
assertThrowsDeadWrapper(() => dbg.disableAsyncStack(g1));
assertThrowsDeadWrapper(() => dbg.enableUnlimitedStacksCapturing(g1));
assertThrowsDeadWrapper(() => dbg.disableUnlimitedStacksCapturing(g1));
