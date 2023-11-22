// reparsing a source should produce equivalent scripts and avoid invoking the
// onNewScript hook.

const g = newGlobal({newCompartment: true});
const dbg = new Debugger;
dbg.addDebuggee(g);

let globalScript;
dbg.onNewScript = script => { globalScript = script };

// Leave `g()` on the first line so we can check that `columnNumber` is passed to the
// reparsed script (`columnNumber` is only used to offset breakpoint column on the first
// line of the script).
g.evaluate(`g();
function f() {
  for (var i = 0; i < 10; i++) {
    g();
  }
}

function g() {
  return 3;
}

f();
`, {
  fileName: "foobar.js",
  lineNumber: 3,
  columnNumber: 42,
});

let onNewScriptCalls = 0;
dbg.onNewScript = script => { onNewScriptCalls++; };

const reparsedScript = globalScript.source.reparse();

assertEq(onNewScriptCalls, 0);

assertEq(reparsedScript.url, "foobar.js");
assertEq(reparsedScript.startLine, 3);
assertEq(reparsedScript.startColumn, 42);

// Test for the same breakpoint positions in the original and reparsed script.
function getBreakpointPositions(script) {
  const offsets = script.getPossibleBreakpoints();
  const str = offsets.map(({ lineNumber, columnNumber }) => `${lineNumber}:${columnNumber}`).toString();
  const childPositions = script.getChildScripts().map(getBreakpointPositions);
  return str + childPositions.toString();
}
assertEq(getBreakpointPositions(globalScript), getBreakpointPositions(reparsedScript));
