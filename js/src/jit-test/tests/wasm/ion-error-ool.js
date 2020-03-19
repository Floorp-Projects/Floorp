// |jit-test| skip-if: !getJitCompilerOptions()['baseline.enable']
// These tests need at least baseline to make sense.

const { assertStackTrace, startProfiling, endProfiling, assertEqPreciseStacks } = WasmHelpers;

const options = getJitCompilerOptions();
const TRIGGER = options['baseline.warmup.trigger'] + 10;
const ITER = 2 * TRIGGER;
const EXCEPTION_ITER = TRIGGER + 5;

const SLOW_ENTRY_STACK = ['', '!>', '0,!>', '!>', ''];
const FAST_ENTRY_STACK = ['', '>', '0,>', '>', ''];
const INLINED_CALL_STACK = ['', '0', ''];
const FAST_OOL_ENTRY_STACK = ['', '>', '<,>', 'ool>,>', '<,>', '>', '0,>', '>', ''];
const EXCEPTION_ENTRY_STACK = ['', '>', '<,>', 'ool>,>', '<,>', '>', ''];

enableGeckoProfiling();

for (let type of ['i32', 'f32', 'f64']) {
    var instance = wasmEvalText(`(module
        (func $add (export "add") (result ${type}) (param ${type}) (param ${type})
         local.get 0
         local.get 1
         ${type}.add
        )
    )`).exports;

    function loopBody(a, b) {
        var caught = null;
        try {
            instance.add(a, b);
        } catch(e) {
            assertEq(e.message, "ph34r");
            assertStackTrace(e, ['innerValueOf', 'outerValueOf', 'loopBody', 'main', '']);
            caught = e;
        }
        assertEq(!!caught, b === EXCEPTION_ITER);
    }

    var x = 0;
    function main() {
        let observedStacks = [0, 0, 0];
        for (var i = 0; i < ITER; i++) {
            startProfiling();
            loopBody(i + 1, i + EXCEPTION_ITER + 1);
            assertEqPreciseStacks(endProfiling(), [INLINED_CALL_STACK, FAST_ENTRY_STACK, SLOW_ENTRY_STACK]);

            if (i === EXCEPTION_ITER) {
                x = { valueOf: function innerValueOf() { throw new Error("ph34r"); }};
            } else {
                x = i;
            }

            startProfiling();
            loopBody({valueOf: function outerValueOf() { return x|0; }}, i);
            let stack = endProfiling();
            let which = assertEqPreciseStacks(stack, [FAST_OOL_ENTRY_STACK, SLOW_ENTRY_STACK, EXCEPTION_ENTRY_STACK]);
            if (which !== null) {
                if (i === EXCEPTION_ITER) {
                    assertEq(which, 2);
                }
                observedStacks[which]++;
            }
        }

        let sum = observedStacks.reduce((acc, x) => acc + x);
        assertEq(sum === 0 || sum === ITER, true);
        if (sum === ITER) {
            assertEq(observedStacks[0] > 0, true, "the fast entry should have been taken at least once");
            assertEq(observedStacks[2], 1, "the error path should have been taken exactly once");
        }
    }

    main();
}

disableGeckoProfiling();
