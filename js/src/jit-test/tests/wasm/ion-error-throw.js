const options = getJitCompilerOptions();

// These tests need at least baseline to make sense.
if (!options['baseline.enable'])
    quit();

const { assertStackTrace, startProfiling, endProfiling, assertEqPreciseStacks } = WasmHelpers;

enableGeckoProfiling();

let { add } = wasmEvalText(`(module
    (func $add (export "add") (result i32) (param i32) (param i32)
     get_local 0
     i32.const 42
     i32.eq
     if
         unreachable
     end

     get_local 0
     get_local 1
     i32.add
    )
)`).exports;

const SLOW_ENTRY_STACK = ['', '!>', '0,!>', '!>', ''];
const FAST_ENTRY_STACK = ['', '>', '0,>', '>', ''];
const INLINED_CALL_STACK = ['', '0', ''];

function main() {
    for (let i = 0; i < 50; i++) {
        startProfiling();
        try {
            assertEq(add(i, i+1), 2*i+1);
        } catch (e) {
            assertEq(i, 42);
            assertEq(e.message.includes("unreachable"), true);
            assertStackTrace(e, ['wasm-function[0]', 'main', '']);
        }
        let stack = endProfiling();
        assertEqPreciseStacks(stack, [INLINED_CALL_STACK, FAST_ENTRY_STACK, SLOW_ENTRY_STACK]);
    }
}

main();
