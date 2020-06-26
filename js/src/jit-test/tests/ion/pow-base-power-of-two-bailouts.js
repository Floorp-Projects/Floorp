// Lowering provides a specialisation when the base operand is a constant which
// is a power of two.
//
// Test bailout conditions for this optimisation.

function test(x) {
    function pow(x, y) { return `Math.pow(${x}, ${y})` };
    function exp(x, y) { return `((${x}) ** ${y})` };

    function make(fn) {
        return Function("y, z", `
            // Load from array to prevent constant-folding.
            // (Ion is currently not smart enough to realise that both array
            // values are the same.)
            var ys = [y, y];
            var zs = [z, z];
            for (var i = 0; i < 1000; ++i) {
                assertEq(${fn(x, "ys[i & 1]")}, zs[i & 1]);
            }
        `);
    }

    function double(v) {
        // NB: numberToDouble() always returns a double value.
        return numberToDouble(v);
    }

    // Find the first power which will exceed the Int32 range by computing ⌈log_x(2 ^ 31)⌉.
    var limit = Math.ceil(Math.log2(2 ** 31) / Math.log2(x));
    assertEq(Math.pow(x, limit - 1) < 2 ** 31, true);
    assertEq(Math.pow(x, limit) >= 2 ** 31, true);

    function* args(first, last) {
        // Run the test function a few times without a bailout.
        for (var i = 0; i < 3; ++i) {
            yield first;
        }

        // |last| should trigger a bailout.
        yield last;
    }

    // Test precision loss when the result exceeds 2**31.
    for (var fn of [make(pow), make(exp)]) {
        for (var y of args(limit - 1, limit)) {
            // Ensure the callee always sees a double to avoid an early Bailout_ArgumentCheck.
            var z = double(Math.pow(x, y));
            fn(y, z);
        }
    }

    // Test precision loss when the result is a fractional number.
    for (var fn of [make(pow), make(exp)]) {
        for (var y of args(0, -1)) {
            // Ensure the callee always sees a double to avoid an early Bailout_ArgumentCheck.
            var z = double(Math.pow(x, y));
            fn(y, z);
        }
    }

    // Find the first negative power which can be represented as a double
    var negLimit = -Math.floor(1074 / Math.log2(x));

    // Test precision loss when the result is a non-zero, fractional number.
    for (var fn of [make(pow), make(exp)]) {
        for (var y of args(limit - 1, limit)) {
            // Ensure the callee always sees a double to avoid an early Bailout_ArgumentCheck.
            var z = double(Math.pow(x, y));
            fn(y, z);
        }
    }
}

function* range(a, b, fn) {
    for (var i = a; i <= b; ++i) {
        yield fn(i);
    }
}

// Only 2^i with |i| ∈ {1..8} currently triggers the optimisation, but also test
// the next power-of-two values.

for (var x of range(1, 10, i => 2 ** i)) {
    test(x);
}
