load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

// Explicitly opt into the new binary format for imports and exports until it
// is used by default everywhere.
const textToBinary = str => wasmTextToBinary(str, 'new-format');

if (!wasmUsesSignalForOOB())
    quit();

function enable() {
    assertEq(getJitCompilerOptions()["signals.enable"], 0);
    setJitCompilerOption("signals.enable", 1);
}
function disable() {
    assertEq(getJitCompilerOptions()["signals.enable"], 1);
    setJitCompilerOption("signals.enable", 0);
}

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Memory = WebAssembly.Memory;
const code = textToBinary('(module (import "x" "y" (memory 1 1)))');

disable();
var mem = new Memory({initial:1});
enable();
var m = new Module(code);
disable();
assertErrorMessage(() => new Instance(m, {x:{y:mem}}), Error, /signals/);
var m = new Module(code);
enable();
assertEq(new Instance(m, {x:{y:mem}}) instanceof Instance, true);
var mem = new Memory({initial:1});
disable();
var m = new Module(code);
enable();
assertEq(new Instance(m, {x:{y:mem}}) instanceof Instance, true);
