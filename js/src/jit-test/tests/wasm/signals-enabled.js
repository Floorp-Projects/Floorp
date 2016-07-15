load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

// Explicitly opt into the new binary format for imports and exports until it
// is used by default everywhere.
const textToBinary = str => wasmTextToBinary(str, 'new-format');

if (!wasmUsesSignalForOOB())
    quit();

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Memory = WebAssembly.Memory;
const code = textToBinary('(module (import "x" "y" (memory 1 1)))');

suppressSignalHandlers(true);
var mem = new Memory({initial:1});
suppressSignalHandlers(false);
var m = new Module(code);
suppressSignalHandlers(true);
assertErrorMessage(() => new Instance(m, {x:{y:mem}}), Error, /signals/);
var m = new Module(code);
suppressSignalHandlers(false);
assertEq(new Instance(m, {x:{y:mem}}) instanceof Instance, true);
var mem = new Memory({initial:1});
suppressSignalHandlers(true);
var m = new Module(code);
suppressSignalHandlers(false);
assertEq(new Instance(m, {x:{y:mem}}) instanceof Instance, true);
