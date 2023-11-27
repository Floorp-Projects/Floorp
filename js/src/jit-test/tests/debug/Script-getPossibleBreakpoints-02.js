
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

  assertBPCount({ line: 6, minColumn: 8 }, 3);
  assertBPCount({ line: 6, maxColumn: 17 }, 3);
  assertBPCount({ line: 6, minColumn: 8, maxColumn: 17 }, 2);
  assertBPError({ line: 1, minLine: 1 }, "line", "not allowed alongside 'minLine'/'maxLine'");
  assertBPError({ line: 1, maxLine: 1 }, "line", "not allowed alongside 'minLine'/'maxLine'");
  assertBPError({ line: "1" }, "line", "not an integer");

  assertBPCount({ minLine: 9 }, 2);
  assertBPCount({ minLine: 9, minColumn: 1 }, 2);
  assertBPCount({ minLine: 9, minColumn: 9 }, 1);
  assertBPError({ minLine: "1" }, "minLine", "not an integer");
  assertBPError({ minColumn: 2 }, "minColumn", "not allowed without 'line' or 'minLine'");
  assertBPError({ minLine: 1, minColumn: "2" }, "minColumn", "not a positive integer");
  assertBPError({ minLine: 1, minColumn: 0 }, "minColumn", "not a positive integer");
  assertBPError({ minLine: 1, minColumn: -1 }, "minColumn", "not a positive integer");

  assertBPCount({ maxLine: 7 }, 5);
  assertBPCount({ maxLine: 7, maxColumn: 1 }, 5);
  assertBPCount({ maxLine: 7, maxColumn: 9 }, 6);
  assertBPError({ maxLine: "1" }, "maxLine", "not an integer");
  assertBPError({ maxColumn: 2 }, "maxColumn", "not allowed without 'line' or 'maxLine'");
  assertBPError({ maxLine: 1, maxColumn: "2" }, "maxColumn", "not a positive integer");
  assertBPError({ maxLine: 1, maxColumn: 0 }, "maxColumn", "not a positive integer");
  assertBPError({ maxLine: 1, maxColumn: -1 }, "maxColumn", "not a positive integer");

  assertBPCount({ minLine: 6, maxLine: 8 }, 6);
  assertBPCount({ minLine: 6, minColumn: 9, maxLine: 8 }, 5);
  assertBPCount({ minLine: 6, maxLine: 8, maxColumn: 9 }, 7);
  assertBPCount({ minLine: 6, minColumn: 9, maxLine: 8, maxColumn: 9 }, 6);

  assertBPCount({
    minOffset: fScript.getPossibleBreakpoints({ line: 6 })[3].offset,
  }, 8);
  assertBPError({ minOffset: "1" }, "minOffset", "not an integer");
  assertBPCount({
    maxOffset: fScript.getPossibleBreakpoints({ line: 6 })[3].offset,
  }, 4);
  assertBPError({ maxOffset: "1" }, "maxOffset", "not an integer");
  assertBPCount({
    minOffset: fScript.getPossibleBreakpoints({ line: 6 })[2].offset,
    maxOffset: fScript.getPossibleBreakpoints({ line: 7 })[1].offset,
  }, 3);

  function assertBPError(query, field, message) {
    try {
      fScript.getPossibleBreakpoints(query);
      assertEq(false, true);
    } catch (err) {
      assertEq(err.message, `getPossibleBreakpoints' '${field}' is ${message}`);
    }
  }

  function assertBPCount(query, count) {
    assertEq(fScript.getPossibleBreakpoints(query).length, count);
  }
};
