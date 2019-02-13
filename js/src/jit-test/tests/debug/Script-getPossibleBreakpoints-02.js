
var global = newGlobal({newCompartment: true});
var dbg = Debugger(global);
dbg.onDebuggerStatement = onDebuggerStatement;

global.eval(`
  debugger;
  function f() {
    var o = {};         // 4

    o.a; o.a; o.a; o.a; // 6
    o.a; o.a;           // 7
    o.a; o.a; o.a;      // 8
    o.a;                // 9
  }                     // 10
`);

function onDebuggerStatement(frame) {
  const fScript = frame.script.getChildScripts()[0];

  const allBreakpoints = fScript.getPossibleBreakpoints();
  assertEq(allBreakpoints.length, 12);

  assertBPCount({ line: 4 }, 1);
  assertBPCount({ line: 5 }, 0);
  assertBPCount({ line: 6 }, 4);
  assertBPCount({ line: 7 }, 2);
  assertBPCount({ line: 8 }, 3);
  assertBPCount({ line: 9 }, 1);
  assertBPCount({ line: 10 }, 1);

  assertBPCount({ line: 6, minColumn: 7 }, 3);
  assertBPCount({ line: 6, maxColumn: 16 }, 3);
  assertBPCount({ line: 6, minColumn: 7, maxColumn: 16 }, 2);

  assertBPCount({ minLine: 9 }, 2);
  assertBPCount({ minLine: 9, minColumn: 0 }, 2);
  assertBPCount({ minLine: 9, minColumn: 8 }, 1);

  assertBPCount({ maxLine: 7 }, 5);
  assertBPCount({ maxLine: 7, maxColumn: 0 }, 5);
  assertBPCount({ maxLine: 7, maxColumn: 8 }, 6);

  assertBPCount({ minLine: 6, maxLine: 8 }, 6);
  assertBPCount({ minLine: 6, minColumn: 8, maxLine: 8 }, 5);
  assertBPCount({ minLine: 6, maxLine: 8, maxColumn: 8 }, 7);
  assertBPCount({ minLine: 6, minColumn: 8, maxLine: 8, maxColumn: 8 }, 6);

  assertBPCount({
    minOffset: fScript.getPossibleBreakpoints({ line: 6 })[3].offset,
  }, 8);
  assertBPCount({
    maxOffset: fScript.getPossibleBreakpoints({ line: 6 })[3].offset,
  }, 4);
  assertBPCount({
    minOffset: fScript.getPossibleBreakpoints({ line: 6 })[2].offset,
    maxOffset: fScript.getPossibleBreakpoints({ line: 7 })[1].offset,
  }, 3);

  function assertBPCount(query, count) {
    assertEq(fScript.getPossibleBreakpoints(query).length, count);
  }
};
