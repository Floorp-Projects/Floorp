const options = getJitCompilerOptions();

// These tests need at least baseline to make sense.
if (!options['baseline.enable'])
    quit();

const TRIGGER = options['baseline.warmup.trigger'] + 10;
const ITER = 2 * TRIGGER;
const EXCEPTION_ITER = TRIGGER + 5;

for (let type of ['i32', 'f32', 'f64']) {
    var instance = wasmEvalText(`(module
        (func $add (export "add") (result ${type}) (param ${type}) (param ${type})
         get_local 0
         get_local 1
         ${type}.add
        )
    )`).exports;

    function loopBody(a, b) {
        var caught = null;
        try {
            instance.add(a, b);
        } catch(e) {
            caught = e;
        }
        assertEq(!!caught, b === EXCEPTION_ITER);
    }

    var x = 0;
    function main() {
        for (var i = 0; i <= EXCEPTION_ITER; i++) {
            loopBody(i + 1, i + EXCEPTION_ITER + 1);

            let otherArg = { valueOf() { return i|0; } };

            if (i === EXCEPTION_ITER) {
                x = { valueOf: function innerValueOf() {
                    // Supress callee.
                    instance = null;
                    // Suppress other arguments.
                    otherArg = null;
                    gc();
                    return 42;
                }};
            } else {
                x = i;
            }

            loopBody({valueOf: function outerValueOf() { return x|0; }}, otherArg);
        }
    }

    main();
}
