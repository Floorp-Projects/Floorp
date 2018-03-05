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

var { table } = wasmEvalText(`(module
    (func $add (result i32) (param i32) (param i32)
     get_local 0
     get_local 1
     i32.add
    )
    (table (export "table") 10 anyfunc)
    (elem (i32.const 0) $add)
)`).exports;

function main() {
    for (var i = 0; i < ITER; i++) {
        startProfiling();
        assertEq(table.get(0)(i, i+1), i*2+1);
        assertEqPreciseStacks(endProfiling(), [FAST_ENTRY_STACK, SLOW_ENTRY_STACK]);
    }
}

enableGeckoProfiling();
main();
disableGeckoProfiling();
