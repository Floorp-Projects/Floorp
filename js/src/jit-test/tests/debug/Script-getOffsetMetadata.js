var global = newGlobal({newCompartment: true});
var dbg = Debugger(global);
dbg.onDebuggerStatement = function(frame) {
  const bps = frame.script.getPossibleBreakpoints();

  const stepBps = [];
  frame.onStep = function() {
    assertOffset(this);
  };

  assertOffset(frame);

  function assertOffset(frame) {
    const meta = frame.script.getOffsetMetadata(frame.offset);

    if (meta.isBreakpoint) {
      assertEq(frame.offset, bps[0].offset);
      const expectedData = bps.shift();

      assertEq(meta.lineNumber, expectedData.lineNumber);
      assertEq(meta.columnNumber, expectedData.columnNumber);
      assertEq(meta.isStepStart, expectedData.isStepStart);
    } else {
      assertEq(meta.isStepStart, false);
    }
  };
};

global.eval(`
  function a() { return "str"; }
  debugger;

  console.log("42" + a());
  a();
  a() + a();
`);
