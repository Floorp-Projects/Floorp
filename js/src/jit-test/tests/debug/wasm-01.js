// Tests that wasm module scripts are available via findScripts.

if (!wasmIsSupported())
  quit();

var g = newGlobal();
g.eval(`o = Wasm.instantiateModule(wasmTextToBinary('(module (func) (export "" 0))'));`);

function isWasm(script) { return script.format === "wasm"; }

var dbg = new Debugger(g);
var foundScripts1 = dbg.findScripts().filter(isWasm);
assertEq(foundScripts1.length, 1);
var found = foundScripts1[0];

// Add another module, we should be able to find it via findScripts.
g.eval(`o2 = Wasm.instantiateModule(wasmTextToBinary('(module (func) (export "a" 0))'));`);
var foundScripts2 = dbg.findScripts().filter(isWasm);
assertEq(foundScripts2.length, 2);

// The first module should be in the list as wrapping the same wasm module
// twice gets the same Debugger.Script.
assertEq(foundScripts2.indexOf(found) !== -1, true);

// The two modules are distinct.
assertEq(foundScripts2[0] !== foundScripts2[1], true);

// We should be able to find the same script via its source.
for (var ws of foundScripts2) {
  var scriptsFromSource = dbg.findScripts({ source: ws.source });
  assertEq(scriptsFromSource.length, 1);
  assertEq(scriptsFromSource[0], ws);
}
