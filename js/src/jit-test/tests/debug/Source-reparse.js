// reparsing a source should produce equivalent scripts and avoid invoking the
// onNewScript hook.

const g = newGlobal({newCompartment: true});
const dbg = new Debugger;
dbg.addDebuggee(g);

let globalScript;
dbg.onNewScript = script => { globalScript = script };

g.evaluate(`
function f() {
  for (var i = 0; i < 10; i++) {
    g();
  }
}

function g() {
  return 3;
}

f();
`, { fileName: "foobar.js", lineNumber: 3 });

let onNewScriptCalls = 0;
dbg.onNewScript = script => { onNewScriptCalls++; };

const reparsedScript = globalScript.source.reparse();

assertEq(onNewScriptCalls, 0);

assertEq(reparsedScript.url, "foobar.js");
assertEq(reparsedScript.startLine, 3);

// Test for the same breakpoint positions in the original and reparsed script.
function getBreakpointPositions(script) {
  const offsets = script.getPossibleBreakpoints();
  const str = offsets.map(({ lineNumber, columnNumber }) => `${lineNumber}:${columnNumber}`).toString();
  const childPositions = script.getChildScripts().map(getBreakpointPositions);
  return str + childPositions.toString();
}
assertEq(getBreakpointPositions(globalScript), getBreakpointPositions(reparsedScript));
