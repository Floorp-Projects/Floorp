// Do not run the test if we're jit-compiling JS, since it's the wasm frames
// we're interested in and eager JS compilation can upset the test.

opts = getJitCompilerOptions();
if (opts['ion.enable'] || opts['baseline.enable'])
  quit();

const { startProfiling, endProfiling, assertEqPreciseStacks, isSingleStepProfilingEnabled } = WasmHelpers;

let e = wasmEvalText(`(module
    (global $g (mut externref) (ref.null extern))
    (func (export "set") (param externref) local.get 0 global.set $g)
)`).exports;

let obj = { field: null };

// GCZeal mode 4 implies that prebarriers are being verified at many
// locations in the interpreter, during interrupt checks, etc. It can be ultra
// slow, so disable it with gczeal(0) when it's not strictly needed.
gczeal(4, 1);
e.set(obj);
e.set(null);
gczeal(0);

if (!isSingleStepProfilingEnabled) {
    quit(0);
}

enableGeckoProfiling();
startProfiling();
gczeal(4, 1);
e.set(obj);
gczeal(0);
assertEqPreciseStacks(
    endProfiling(),
    [
        // Expected output for (simulator+via-Ion).
        ['', '!>', '0,!>', '<,0,!>', 'filtering GC postbarrier,0,!>',
         '<,0,!>', '0,!>', '!>', ''],

        // Expected output for (simulator+baseline).
        ['', '!>', '0,!>', '<,0,!>', 'GC postbarrier,0,!>',
         '<,0,!>', '0,!>', '!>', ''],

        // Expected output for other configurations.
        ['', '!>', '0,!>', '!>', ''],
    ]);

startProfiling();
gczeal(4, 1);
e.set(null);
gczeal(0);

// We're losing stack info in the prebarrier code.
assertEqPreciseStacks(
    endProfiling(),
    [
        // Expected output for (simulator+via-Ion).
        ['', '!>', '0,!>', '', '0,!>', '<,0,!>', 'filtering GC postbarrier,0,!>',
         '<,0,!>', '0,!>', '!>', ''],

        // Expected output for other configurations.
        ['', '!>', '0,!>', '', '0,!>', '!>', ''],
    ]);

