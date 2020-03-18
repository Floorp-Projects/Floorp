// |jit-test| skip-if: !wasmDebuggingIsSupported()

// Tests that wasm module scripts throw for everything except text.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

var s;
dbg.onNewScript = (script) => {
  s = script;
}

g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func) (export "" (func 0)))')));`);
assertEq(s.format, "wasm");

assertThrowsInstanceOf(() => s.displayName, Error);
assertThrowsInstanceOf(() => s.url, Error);
assertThrowsInstanceOf(() => s.sourceStart, Error);
assertThrowsInstanceOf(() => s.sourceLength, Error);
assertThrowsInstanceOf(() => s.global, Error);
assertThrowsInstanceOf(() => s.getChildScripts(), Error);
assertThrowsInstanceOf(() => s.getAllOffsets(), Error);
assertThrowsInstanceOf(() => s.getBreakpoint(0), Error);
assertThrowsInstanceOf(() => s.getOffsetsCoverage(), Error);
