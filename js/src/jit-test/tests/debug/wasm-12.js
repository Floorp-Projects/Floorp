// Tests that wasm module scripts have special URLs.

if (!wasmIsSupported())
  quit();

var g = newGlobal();
g.eval(`
function initWasm(s) { return new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(s))); }
o = initWasm('(module (func) (export "" 0))');
o = initWasm('(module (func) (func) (export "" 1))');
`);

function isWasm(script) { return script.format === "wasm"; }

function isValidWasmURL(url) {
   // The URLs will have the following format:
   //   wasm: [<uri-econded-filename-of-host> ":"] <64-bit-hash>
   return /^wasm:(?:[^:]*:)*?[0-9a-f]{16}$/.test(url);
}

var dbg = new Debugger(g);
var foundScripts = dbg.findScripts().filter(isWasm);
assertEq(foundScripts.length, 2);
assertEq(isValidWasmURL(foundScripts[0].source.url), true);
assertEq(isValidWasmURL(foundScripts[1].source.url), true);
assertEq(foundScripts[0].source.url != foundScripts[1].source.url, true);
