// |jit-test| skip-if: !wasmReftypesEnabled()

const { startProfiling, endProfiling, assertEqPreciseStacks, isSingleStepProfilingEnabled } = WasmHelpers;

// Dummy constructor.
function Baguette(calories) {
    this.calories = calories;
}

// Ensure the baseline compiler sync's before the postbarrier.
(function() {
    wasmEvalText(`(module
        (global (mut anyref) (ref.null))
        (func (export "f")
            global.get 0
            ref.null
            global.set 0
            global.set 0
        )
    )`).exports.f();
})();

let exportsPlain = wasmEvalText(`(module
    (global i32 (i32.const 42))
    (global $g (mut anyref) (ref.null))
    (func (export "set") (param anyref) local.get 0 global.set $g)
    (func (export "get") (result anyref) global.get $g)
)`).exports;

let exportsObj = wasmEvalText(`(module
    (global $g (export "g") (mut anyref) (ref.null))
    (func (export "set") (param anyref) local.get 0 global.set $g)
    (func (export "get") (result anyref) global.get $g)
)`).exports;

// 7 => Generational GC zeal.
gczeal(7, 1);

for (var i = 0; i < 100; i++) {
    new Baguette(i);
}

function test(exports) {
    // Test post-write barrier in wasm code.
    {
        let nomnom = new Baguette(15);
        exports.set(nomnom);
        nomnom = null;
    }
    new Baguette();
    assertEq(exports.get().calories, 15);
}

test(exportsPlain);
test(exportsObj);

// Test stacks reported in profiling mode in a separate way, to not perturb
// the behavior of the tested functions.
if (!isSingleStepProfilingEnabled)
    quit(0);

enableGeckoProfiling();

const EXPECTED_STACKS = [
    // Expected output for (simulator+baseline).
    ['', '!>', '0,!>', '<,0,!>', 'GC postbarrier,0,!>',
     '<,0,!>', '0,!>', '!>', ''],

    // Expected output for (simulator+via-Ion).
    ['', '!>', '0,!>', '<,0,!>', 'filtering GC postbarrier,0,!>',
     '<,0,!>', '0,!>', '!>', ''],

    // Expected output for other configurations.
    ['', '!>', '0,!>', '!>', ''],
];

function testStacks(exports) {
    // Test post-write barrier in wasm code.
    {
        let nomnom = new Baguette(15);
        startProfiling();
        exports.set(nomnom);
        assertEqPreciseStacks(endProfiling(), EXPECTED_STACKS);
        nomnom = null;
    }
    new Baguette();
    assertEq(exports.get().calories, 15);
}

testStacks(exportsPlain);
testStacks(exportsObj);
