if (!wasmGcEnabled()) {
    quit(0);
}

const { startProfiling, endProfiling, assertEqPreciseStacks, isSingleStepProfilingEnabled } = WasmHelpers;

let e = wasmEvalText(`(module
    (global $g (mut anyref) (ref.null anyref))
    (func (export "set") (param anyref) get_local 0 set_global $g)
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
assertEqPreciseStacks(endProfiling(), [['', '!>', '0,!>', '!>', '']]);

startProfiling();
gczeal(4, 1);
e.set(null);
gczeal(0);

// We're losing stack info in the prebarrier code.
assertEqPreciseStacks(endProfiling(), [['', '!>', '0,!>', '', '0,!>', '!>', '']]);
