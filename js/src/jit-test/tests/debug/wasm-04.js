// Tests that wasm module scripts throw for everything except text.

load(libdir + "asserts.js");

if (!wasmIsSupported())
  quit();

var g = newGlobal();
var dbg = new Debugger(g);

var s;
dbg.onNewScript = (script) => {
  s = script;
}

g.eval(`o = Wasm.instantiateModule(wasmTextToBinary('(module (func) (export "" 0))'));`);
assertEq(s.format, "wasm");

assertThrowsInstanceOf(() => s.displayName, Error);
assertThrowsInstanceOf(() => s.url, Error);
assertThrowsInstanceOf(() => s.startLine, Error);
assertThrowsInstanceOf(() => s.lineCount, Error);
assertThrowsInstanceOf(() => s.sourceStart, Error);
assertThrowsInstanceOf(() => s.sourceLength, Error);
assertThrowsInstanceOf(() => s.global, Error);
assertThrowsInstanceOf(() => s.getChildScripts(), Error);
assertThrowsInstanceOf(() => s.getAllOffsets(), Error);
assertThrowsInstanceOf(() => s.getAllColumnOffsets(), Error);
assertThrowsInstanceOf(() => s.setBreakpoint(0, { hit: () => {} }), Error);
assertThrowsInstanceOf(() => s.getBreakpoint(0), Error);
assertThrowsInstanceOf(() => s.clearBreakpoint({}), Error);
assertThrowsInstanceOf(() => s.clearAllBreakpoints(), Error);
assertThrowsInstanceOf(() => s.isInCatchScope(0), Error);
assertThrowsInstanceOf(() => s.getOffsetsCoverage(), Error);
