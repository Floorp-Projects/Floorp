const options = getJitCompilerOptions();

// These tests need at least baseline to make sense.
if (!options['baseline.enable'])
    quit();

const { assertStackTrace, startProfiling, endProfiling, assertEqPreciseStacks } = WasmHelpers;

const TRIGGER = options['baseline.warmup.trigger'] + 10;
const ITER = 2 * TRIGGER;
const EXCEPTION_ITER = TRIGGER + 5;

const SLOW_ENTRY_STACK = ['', '!>', '0,!>', '!>', ''];
const FAST_ENTRY_STACK = ['', '>', '0,>', '>', ''];
const INLINED_CALL_STACK = ['', '0', ''];
const EXPECTED_STACKS = [SLOW_ENTRY_STACK, FAST_ENTRY_STACK, INLINED_CALL_STACK];

function main() {
    var { table } = wasmEvalText(`(module
        (func $add (result i32) (param i32) (param i32)
         get_local 0
         get_local 1
         i32.add
        )
        (table (export "table") 10 anyfunc)
        (elem (i32.const 0) $add)
    )`).exports;

    for (var i = 0; i < ITER; i++) {
        startProfiling();
        assertEq(table.get(0)(i, i+1), i*2+1);
        assertEqPreciseStacks(endProfiling(), EXPECTED_STACKS);
    }
}

function withTier2() {
    setJitCompilerOption('wasm.delay-tier2', 1);

    var module = new WebAssembly.Module(wasmTextToBinary(`(module
        (func $add (result i32) (param i32) (param i32)
         get_local 0
         get_local 1
         i32.add
        )
        (table (export "table") 10 anyfunc)
        (elem (i32.const 0) $add)
    )`));
    var { table } = new WebAssembly.Instance(module).exports;

    let i = 0;
    do {
        i++;
        startProfiling();
        assertEq(table.get(0)(i, i+1), i*2+1);
        assertEqPreciseStacks(endProfiling(), EXPECTED_STACKS);
    } while (!wasmHasTier2CompilationCompleted(module));

    for (i = 0; i < ITER; i++) {
        startProfiling();
        assertEq(table.get(0)(i, i+1), i*2+1);
        assertEqPreciseStacks(endProfiling(), EXPECTED_STACKS);
    }

    setJitCompilerOption('wasm.delay-tier2', 0);
}

enableGeckoProfiling();
main();
withTier2();
disableGeckoProfiling();
