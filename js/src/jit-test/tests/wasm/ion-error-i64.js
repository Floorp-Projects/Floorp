const options = getJitCompilerOptions();

// These tests need at least baseline to make sense.
if (!options['baseline.enable'])
    quit();

const { nextLineNumber, startProfiling, endProfiling, assertEqPreciseStacks } = WasmHelpers;

const TRIGGER = options['ion.warmup.trigger'] + 10;
const ITER = 2 * TRIGGER;
const EXCEPTION_ITER = ITER - 2;

enableGeckoProfiling();

var instance = wasmEvalText(`(module
    (func $add (export "add") (result i32) (param i32) (param i32)
     get_local 0
     get_local 1
     i32.add
    )

    (func $addi64 (export "add64") (result i64) (param i32) (param i32)
     get_local 0
     get_local 1
     call $add
     i64.extend_s/i32
    )
)`).exports;

var callToMain;

function main() {
    var arrayCallLine = nextLineNumber(13);
    for (var i = 0; i < ITER; i++) {
        var arr = [instance.add, (x,y)=>x+y];
        if (i === EXCEPTION_ITER) {
            arr[0] = instance.add64;
        } else if (i === EXCEPTION_ITER + 1) {
            arr[0] = instance.add;
        }

        var caught = null;

        startProfiling();
        try {
            arr[i%2](i, i);
        } catch(e) {
            caught = e;
        }
        let profilingStack = endProfiling();

        assertEq(!!caught, i === EXCEPTION_ITER);
        if (caught) {
            assertEqPreciseStacks(profilingStack, [
                // Error stack: control flow is redirected to a builtin thunk
                // then calling into C++ from the wasm entry before jumping to
                // the wasm jit entry exception handler.
                ['', '>', '<,>', 'i64>,>', '<,>', '>', ''],
                [''] // the jit path wasn't taken (interpreter/baseline only).
            ]);

            assertEq(caught.message, 'cannot pass i64 to or from JS');

            let stack = caught.stack.split('\n');

            // Which callsites appear on the error stack.
            let callsites = stack.map(s => s.split('@')[0]);
            assertEq(callsites[0], 'main');
            assertEq(callsites[1], ''); // global scope

            // Which line numbers appear in the error stack.
            let lines = stack.map(s => s.split(':')[1]);
            assertEq(+lines[0], arrayCallLine);
            assertEq(+lines[1], callToMain);
        } else if ((i % 2) == 0) {
            assertEqPreciseStacks(profilingStack, [
                ['', '>', '0,>', '>', ''],    // fast path
                ['', '!>', '0,!>', '!>', ''], // slow path
            ]);
        }
    }
}

callToMain = nextLineNumber();
main();
