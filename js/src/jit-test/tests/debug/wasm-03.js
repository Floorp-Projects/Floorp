// Tests that wasm module scripts have synthesized sources.

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

var source = s.source;

assertEq(s.source, source);
assertEq(source.introductionType, "wasm");
assertEq(source.introductionScript, s);
// Wasm sources shouldn't be considered source mapped.
assertEq(!source.sourceMapURL, true);
assertThrowsInstanceOf(() => source.sourceMapURL = 'foo', Error);
// We must have some text.
assertEq(!!source.text, true);

// TODOshu: Wasm is moving very fast and what we return for these values is
// currently not interesting to test. Instead, test that they do not throw.
source.url;
source.element;
source.displayURL;
source.introductionOffset;
source.elementAttributeName;

// canonicalId doesn't make sense since wasm sources aren't backed by
// ScriptSource.
assertThrowsInstanceOf(() => source.canonicalId, Error);
